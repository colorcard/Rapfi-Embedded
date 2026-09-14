import Foundation

enum Side: Int {
    case black = 1
    case white = 2
    var opposite: Side { self == .black ? .white : .black }
    var name: String { self == .black ? "黑" : "白" }
}

enum GameStatus: Equatable {
    case playing
    case draw
    case humanWin
    case engineWin
}

struct Move: Equatable {
    let x: Int
    let y: Int
}

struct EngineInfo: Equatable {
    var depth = 0
    var score = 0
    var nodes = 0
    var ms = 0
}

/// 对局状态 + MCU 文本协议。
@MainActor
final class GameViewModel: ObservableObject {
    static let n = 15

    @Published var board: [[Int]] = Array(
        repeating: Array(repeating: 0, count: GameViewModel.n), count: GameViewModel.n)
    @Published var turn: Side = .black
    @Published var human: Side = .black
    @Published var depth = 6
    @Published var thinkMs = 2000
    @Published var status: GameStatus = .playing
    @Published var info: EngineInfo?
    @Published var lastMove: Move?
    @Published var connected = false
    @Published var connecting = false
    @Published var portPath = ""
    @Published var deviceName = ""
    @Published var serial = ""
    @Published var errorText: String?

    var thinking: Bool { pending == .go }

    private enum Pending { case none, play, go }
    private var pending: Pending = .none
    private var pendingMove: (Int, Int)?
    private var port: SerialPort?
    private var monitor: Timer?

    // MARK: - 连接

    func autoConnect() {
        guard !connected, !connecting else { return }
        connecting = true
        errorText = nil

        // 优先按 USB VID:PID 精确识别。
        if let dev = USBMatcher.find() {
            attach(path: dev.path, product: dev.product, serial: dev.serial)
            return
        }

        // 回退：逐个串口发 TURN 试探（非本机 USB 设备时）。
        let cands = SerialPort.candidates()
        DispatchQueue.global().async {
            var found: String?
            for path in cands where SerialPort.probe(path) {
                found = path
                break
            }
            DispatchQueue.main.async {
                if let path = found {
                    self.attach(path: path)
                } else {
                    self.connecting = false
                    self.errorText = cands.isEmpty
                        ? "未发现 USB 串口设备"
                        : "未找到 STM32 引擎（VID 0x0483:0x5740）"
                }
            }
        }
    }

    /// 定时检查热插拔：设备出现自动连接，拔出自动断开。
    func startMonitoring() {
        guard monitor == nil else { return }
        monitor = Timer.scheduledTimer(withTimeInterval: 1.5, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.pollUSB() }
        }
    }

    private func pollUSB() {
        if connected {
            if !FileManager.default.fileExists(atPath: portPath) {
                port?.close()
                port = nil
                connected = false
                deviceName = ""
                serial = ""
                errorText = "设备已断开"
            }
        } else if !connecting, USBMatcher.find() != nil {
            autoConnect()
        }
    }

    func attach(path: String, product: String = "", serial sn: String = "") {
        let p = SerialPort()
        guard p.open(path) else {
            connecting = false
            errorText = "无法打开 \(path)"
            return
        }
        port = p
        portPath = path
        deviceName = product
        serial = sn
        p.onLine = { [weak self] line in
            Task { @MainActor in self?.handle(line) }
        }
        p.startReading()
        connected = true
        connecting = false
        errorText = nil
        send("NEW")
        resetLocal()
        send("STATUS")
    }

    private func send(_ cmd: String) { port?.writeLine(cmd) }

    // MARK: - 操作

    func newGame(human newHuman: Side? = nil) {
        if let h = newHuman { human = h }
        send("NEW")
        resetLocal()
        if turn == engineSide { askEngine() }
    }

    func swapSides() { newGame(human: human.opposite) }

    private func resetLocal() {
        board = Array(repeating: Array(repeating: 0, count: Self.n), count: Self.n)
        turn = .black
        status = .playing
        info = nil
        lastMove = nil
        pending = .none
        pendingMove = nil
    }

    var engineSide: Side { human.opposite }

    func click(x: Int, y: Int) {
        guard status == .playing, pending == .none, turn == human else { return }
        guard (0..<Self.n).contains(x), (0..<Self.n).contains(y) else { return }
        guard board[y][x] == 0 else { return }
        pending = .play
        pendingMove = (x, y)
        send("PLAY \(x) \(y)")
    }

    func undo() {
        guard pending == .none else { return }
        send("UNDO")
        send("UNDO")
        send("STATUS")
        board = Array(repeating: Array(repeating: 0, count: Self.n), count: Self.n)
        lastMove = nil
        info = nil
        status = .playing
        turn = human
        pendingMove = nil
    }

    func adjustDepth(_ delta: Int) {
        depth = max(1, min(12, depth + delta))
    }

    private func askEngine() {
        pending = .go
        send("GO \(depth) \(thinkMs)")
    }

    // MARK: - 协议解析

    private func handle(_ line: String) {
        if line == "OK" {
            if pending == .play {
                pending = .none
                if let (x, y) = pendingMove {
                    if board[y][x] == 0 {
                        board[y][x] = human.rawValue
                        lastMove = Move(x: x, y: y)
                    }
                    pendingMove = nil
                }
                turn = turn.opposite
                send("STATUS")
            }
            return
        }
        if line == "ERR" {
            pending = .none
            pendingMove = nil
            return
        }
        if line.hasPrefix("MOVE ") {
            let t = line.split(separator: " ").compactMap { Int($0) }
            if t.count == 6 {
                let (x, y, score, dep, nodes, ms) = (t[0], t[1], t[2], t[3], t[4], t[5])
                if (0..<Self.n).contains(x), (0..<Self.n).contains(y), board[y][x] == 0 {
                    board[y][x] = engineSide.rawValue
                    lastMove = Move(x: x, y: y)
                }
                info = EngineInfo(depth: dep, score: score, nodes: nodes, ms: ms)
            }
            turn = human
            pending = .none
            send("STATUS")
            return
        }
        if line.hasPrefix("STATUS ") {
            let st = String(line.dropFirst(7)).trimmingCharacters(in: .whitespaces)
            if st.hasPrefix("WIN") {
                let win: Side = st.contains("B") ? .black : .white
                status = (win == human) ? .humanWin : .engineWin
            } else if st == "DRAW" {
                status = .draw
            } else if st == "PLAYING" {
                if pending == .none, status == .playing, turn == engineSide {
                    askEngine()
                }
            }
        }
    }
}

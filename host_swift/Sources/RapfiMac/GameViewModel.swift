import Foundation
import RapfiKit

enum Side: Int {
    case black = 1
    case white = 2
    var opposite: Side { self == .black ? .white : .black }
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

/// 一手记录（含引擎分析），用于日志与分析曲线。
struct MoveRecord: Identifiable, Equatable {
    let id = UUID()
    let index: Int
    let side: Side
    let x: Int
    let y: Int
    var score: Int?
    var depth: Int?
    var nodes: Int?
    var ms: Int?

    var coord: String {
        let col = String(UnicodeScalar(UInt8(65 + x)))
        return "\(col)\(y + 1)"
    }
    /// 白方视角评分（+ 表示白优）；仅引擎手有值。
    var whiteScore: Int? {
        guard let s = score else { return nil }
        return side == .white ? s : -s
    }
}

/// 分析曲线点。
struct EvalPoint: Identifiable, Equatable {
    let id = UUID()
    let move: Int
    let value: Int
}

/// 对局状态 + 定长二进制协议（见固件 project/code/rapfi_protocol.h）。
@MainActor
final class GameViewModel: ObservableObject {
    static let n = 15

    private enum Cmd {
        static let newGame: UInt8 = 0x01
        static let play: UInt8 = 0x02
        static let go: UInt8 = 0x03
        static let undo: UInt8 = 0x04
        static let status: UInt8 = 0x05
        static let turn: UInt8 = 0x06
        static let ping: UInt8 = 0x07
    }
    private enum Rsp {
        static let ok: UInt8 = 0x80
        static let err: UInt8 = 0x81
        static let move: UInt8 = 0x82
        static let status: UInt8 = 0x83
        static let turn: UInt8 = 0x84
        static let ready: UInt8 = 0x85
    }
    private static let rspLen: [UInt8: Int] =
        [0x80: 3, 0x81: 1, 0x82: 18, 0x83: 3, 0x84: 2, 0x85: 1]

    private enum Pending { case none, newGame, play, go, undo }

    @Published var board: [[Int]] = Array(
        repeating: Array(repeating: 0, count: n), count: n)
    @Published var turn: Side = .black
    @Published var human: Side = .black
    @Published var depth = 6
    @Published var thinkMs = 2000
    @Published var status: GameStatus = .playing
    @Published var info: EngineInfo?
    @Published var lastMove: Move?
    @Published var moves: [MoveRecord] = []
    @Published var connected = false
    @Published var connecting = false
    @Published var portPath = ""
    @Published var deviceName = ""
    @Published var serial = ""
    @Published var errorText: String?
    @Published var exportedPath: String?

    var thinking: Bool { pending == .go }
    /// 有待处理命令时锁定操作（悔棋等）。
    var pendingLock: Bool { pending != .none }
    var engineSide: Side { human.opposite }

    /// 分析曲线（白方视角，钳位到 ±2000 便于显示）。
    var evalPoints: [EvalPoint] {
        moves.compactMap { m in
            guard let w = m.whiteScore else { return nil }
            return EvalPoint(move: m.index, value: max(-2000, min(2000, w)))
        }
    }

    private var pending: Pending = .none
    private var pendingMove: (Int, Int)?
    private var port: SerialPort?
    private var monitor: Timer?
    private var rx = Data()

    // MARK: - 连接

    func autoConnect() {
        guard !connected, !connecting else { return }
        connecting = true
        errorText = nil
        if let dev = USBMatcher.find() {
            attach(path: dev.path, product: dev.product, serial: dev.serial)
            return
        }
        let cands = SerialPort.candidates()
        DispatchQueue.global().async {
            var found: String?
            for path in cands where SerialPort.probe(path) { found = path; break }
            DispatchQueue.main.async {
                if let path = found {
                    self.attach(path: path)
                } else {
                    self.connecting = false
                    self.errorText = cands.isEmpty ? "未发现 USB 串口设备"
                        : "未找到 Rapfi 引擎（VID 0x0483:0x5250）"
                }
            }
        }
    }

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
        rx.removeAll()
        p.onBytes = { [weak self] data in
            Task { @MainActor in self?.ingest(data) }
        }
        p.startReading()
        connected = true
        connecting = false
        errorText = nil
        newGame()
    }

    // MARK: - 发送

    private func sendFrame(_ op: UInt8, _ a: UInt8 = 0, _ b: UInt8 = 0, _ c: UInt8 = 0) {
        port?.write(Data([op, a, b, c]))
    }

    // MARK: - 操作

    func newGame(human newHuman: Side? = nil) {
        if let h = newHuman { human = h }
        board = Array(repeating: Array(repeating: 0, count: Self.n), count: Self.n)
        turn = .black
        status = .playing
        info = nil
        lastMove = nil
        moves.removeAll()
        pendingMove = nil
        pending = .newGame
        sendFrame(Cmd.newGame)
    }

    func swapSides() { newGame(human: human.opposite) }

    func click(x: Int, y: Int) {
        guard status == .playing, pending == .none, turn == human else { return }
        guard (0..<Self.n).contains(x), (0..<Self.n).contains(y) else { return }
        guard board[y][x] == 0 else { return }
        pending = .play
        pendingMove = (x, y)
        sendFrame(Cmd.play, UInt8(x), UInt8(y))
    }

    /// 悔棋：撤掉最近两手（引擎一手 + 人一手），回到人类行棋。
    func undo() {
        guard pending == .none, !moves.isEmpty else { return }
        let n = min(2, moves.count)
        pending = .undo
        pendingMove = nil
        sendFrame(Cmd.undo)
        if n == 2 { sendFrame(Cmd.undo) }
        for _ in 0..<n { moves.removeLast() }
        rebuildLocal()
        status = .playing
        turn = human
        info = nil
    }

    /// 按着法历史重建本地棋盘（悔棋/重连后保证与引擎一致）。
    private func rebuildLocal() {
        board = Array(repeating: Array(repeating: 0, count: Self.n), count: Self.n)
        for m in moves { board[m.y][m.x] = m.side.rawValue }
        lastMove = moves.last.map { Move(x: $0.x, y: $0.y) }
    }

    func adjustDepth(_ delta: Int) { depth = max(1, min(12, depth + delta)) }

    private func askEngine() {
        pending = .go
        sendFrame(Cmd.go, UInt8(depth & 0xFF),
                  UInt8(thinkMs & 0xFF), UInt8((thinkMs >> 8) & 0xFF))
    }

    // MARK: - 日志导出

    /// 导出对局日志到 ~/Downloads，返回文件路径。
    @discardableResult
    func exportLog() -> String? {
        let df = DateFormatter()
        df.dateFormat = "yyyyMMdd-HHmmss"
        let url = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Downloads/rapfi-log-\(df.string(from: Date())).txt")
        var text = "# Rapfi-Embedded 对局日志\n"
        text += "时间: \(Date())\n"
        text += "设备: \(deviceName)  \(portPath)\n"
        text += "人类执: \(human == .black ? "黑" : "白")   搜索: depth \(depth) / \(thinkMs)ms\n\n"
        text += "手数\t方\t坐标\t评分\t深度\t节点\t耗时ms\n"
        for m in moves {
            let sc = m.score.map(String.init) ?? "-"
            let dp = m.depth.map(String.init) ?? "-"
            let nd = m.nodes.map(String.init) ?? "-"
            let ms = m.ms.map(String.init) ?? "-"
            text += "\(m.index)\t\(m.side == .black ? "黑" : "白")\t\(m.coord)\t\(sc)\t\(dp)\t\(nd)\t\(ms)\n"
        }
        do {
            try text.write(to: url, atomically: true, encoding: .utf8)
            exportedPath = url.path
            return url.path
        } catch {
            errorText = "导出失败: \(error.localizedDescription)"
            return nil
        }
    }

    // MARK: - 收帧

    private func ingest(_ data: Data) {
        rx.append(data)
        while let first = rx.first {
            guard let n = Self.rspLen[first] else {
                rx.removeFirst()
                continue
            }
            if rx.count < n { break }
            let frame = [UInt8](rx.prefix(n))
            rx.removeFirst(n)
            handleFrame(frame)
        }
    }

    private func u32(_ f: [UInt8], _ o: Int) -> UInt32 {
        UInt32(f[o]) | (UInt32(f[o + 1]) << 8)
            | (UInt32(f[o + 2]) << 16) | (UInt32(f[o + 3]) << 24)
    }

    private func applyState(_ s: UInt8, _ t: UInt8) {
        turn = (t == 0) ? .black : .white
        switch s {
        case 1: status = (human == .black) ? .humanWin : .engineWin
        case 2: status = (human == .white) ? .humanWin : .engineWin
        case 3: status = .draw
        default: status = .playing
        }
    }

    private func handleFrame(_ f: [UInt8]) {
        switch f[0] {
        case Rsp.ok:
            if pending == .play {
                pending = .none
                if let (x, y) = pendingMove, board[y][x] == 0 {
                    board[y][x] = human.rawValue
                    lastMove = Move(x: x, y: y)
                    moves.append(MoveRecord(index: moves.count + 1, side: human,
                                            x: x, y: y))
                }
                pendingMove = nil
                applyState(f[1], f[2])
                if status == .playing, turn == engineSide { askEngine() }
            } else if pending == .newGame {
                pending = .none
                applyState(f[1], f[2])
                if turn == engineSide { askEngine() }
            } else if pending == .undo {
                pending = .none
                applyState(f[1], f[2])
            }
        case Rsp.err:
            pending = .none
            pendingMove = nil
        case Rsp.move:
            let x = Int(f[1]), y = Int(f[2]), dep = Int(f[3])
            let score = Int(Int32(bitPattern: u32(f, 4)))
            let nodes = Int(u32(f, 8)), ms = Int(u32(f, 12))
            if (0..<Self.n).contains(x), (0..<Self.n).contains(y), board[y][x] == 0 {
                board[y][x] = engineSide.rawValue
                lastMove = Move(x: x, y: y)
                moves.append(MoveRecord(index: moves.count + 1, side: engineSide,
                                        x: x, y: y, score: score, depth: dep,
                                        nodes: nodes, ms: ms))
            }
            info = EngineInfo(depth: dep, score: score, nodes: nodes, ms: ms)
            pending = .none
            applyState(f[16], f[17])
        case Rsp.status:
            applyState(f[1], f[2])
        case Rsp.turn:
            turn = (f[1] == 0) ? .black : .white
        default:
            break
        }
    }
}

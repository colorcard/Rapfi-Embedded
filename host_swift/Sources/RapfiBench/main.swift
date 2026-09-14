// rapfi-bench —— STM32 引擎棋力评测（无界面）
//
// 让 STM32 引擎与参考对手（或另一配置的 STM32）对弈 N 局，交替先后手，
// 统计胜/负/和并换算 Elo 差与置信区间。
//
// 用法示例：
//   swift run -c release rapfi-bench --games 100 --depth 6 --time 1000 --opponent greedy
//   swift run -c release rapfi-bench --games 100 --depth 6 --time 1000 --opponent ab2 --anchor-elo 1000
//   swift run -c release rapfi-bench --games 60 --depth 8 --time 2000 --opponent self --opp-depth 4 --opp-time 300
import Foundation
import RapfiKit

setbuf(stdout, nil)

let RSP_LEN: [UInt8: Int] = [0x80: 3, 0x81: 1, 0x82: 18, 0x83: 3, 0x84: 2, 0x85: 1]

// ------------------------------ STM32 通道 ------------------------------

final class Stm32 {
    let port: SerialPort
    var rx = Data()

    init(port: SerialPort) { self.port = port }

    func send(_ op: UInt8, _ a: UInt8 = 0, _ b: UInt8 = 0, _ c: UInt8 = 0) {
        port.write(Data([op, a, b, c]))
    }

    func readFrame(timeoutMs: Int) -> [UInt8]? {
        let deadline = Date().addingTimeInterval(Double(timeoutMs) / 1000.0)
        while Date() < deadline {
            if !rx.isEmpty {
                if rx[rx.startIndex] == 0x86 {
                    /* PV 帧：跳过（变长 2+n） */
                    if rx.count >= 2 {
                        let n = Int(rx[rx.index(rx.startIndex, offsetBy: 1)])
                        if rx.count >= 2 + n {
                            rx.removeFirst(2 + n)
                            continue
                        }
                    }
                } else if let n = RSP_LEN[rx[rx.startIndex]] {
                    if rx.count >= n {
                        let f = [UInt8](rx.prefix(n))
                        rx.removeFirst(n)
                        return f
                    }
                } else {
                    rx.removeFirst() // 未知类型，重新同步
                }
            }
            let d = port.readSync(timeoutMs: 20)
            if !d.isEmpty { rx.append(d) }
        }
        return nil
    }
}

// ------------------------------ 走子方 ------------------------------

protocol Mover {
    var label: String { get }
    func move(_ board: Board, side: Int, stm: Stm32) -> (Int, Int)?
}

/// STM32 引擎（发 GO，读 MOVE）。
struct StmMover: Mover {
    let depth: Int
    let ms: Int
    var label: String { "STM32(d\(depth)/\(ms)ms)" }

    func move(_ board: Board, side: Int, stm: Stm32) -> (Int, Int)? {
        stm.send(0x03, UInt8(depth & 0xFF), UInt8(ms & 0xFF), UInt8((ms >> 8) & 0xFF))
        let wait = (ms > 0 ? ms : 30000) + 5000
        let deadline = Date().addingTimeInterval(Double(wait) / 1000.0)
        var f: [UInt8]?
        while Date() < deadline {
            guard let g = stm.readFrame(timeoutMs: 300) else { continue }
            if g[0] == 0x82 { f = g; break }   /* 等到 MOVE，忽略 OK/READY/PV */
        }
        guard let f = f else { return nil }
        if verbose {
            let sc = Int32(bitPattern: UInt32(f[4]) | (UInt32(f[5]) << 8)
                           | (UInt32(f[6]) << 16) | (UInt32(f[7]) << 24))
            print("     └ d\(f[3]) score=\(sc) [\(f[1]),\(f[2])]")
        }
        return (Int(f[1]), Int(f[2]))
    }
}

/// 本地参考引擎（算好后发 PLAY，让 STM32 同步棋盘）。
struct RefMover: Mover {
    let engine: RefEngine
    var label: String { "Ref-\(engine.level.rawValue)" }

    func move(_ board: Board, side: Int, stm: Stm32) -> (Int, Int)? {
        guard let mv = engine.bestMove(board, side: side) else { return nil }
        stm.send(0x02, UInt8(mv.0), UInt8(mv.1))
        let deadline = Date().addingTimeInterval(3.0)
        while Date() < deadline {
            guard let g = stm.readFrame(timeoutMs: 300) else { continue }
            if g[0] == 0x80 { return mv }      /* 等到 OK */
            if g[0] == 0x81 { return nil }
        }
        return nil
    }
}

// ------------------------------ 对局 ------------------------------

/// 返回 1=黑胜, -1=白胜, 0=平/异常。
func playGame(stm: Stm32, black: Mover, white: Mover) -> Int {
    stm.rx.removeAll()
    stm.send(0x01) // NEW
    _ = stm.readFrame(timeoutMs: 3000)
    var board = Board()
    var side = 1
    for _ in 0..<Board.cells {
        let mover = side == 1 ? black : white
        let t0 = Date()
        guard let mv = mover.move(board, side: side, stm: stm) else {
            if verbose { print("   >> \(side == 1 ? "黑" : "白")(\(mover.label)) 无响应/错误") }
            return 0
        }
        let dt = Date().timeIntervalSince(t0)
        guard board.inBounds(mv.0, mv.1), board[mv.0, mv.1] == 0 else {
            if verbose { print("   >> 非法着法 \(side): \(mv)") }
            return 0
        }
        if verbose {
            let letter = Character(UnicodeScalar(UInt8(65 + mv.0)))
            let tag = (side == 1) ? "黑" : "白"
            print("   \(tag) \(letter)\(mv.1 + 1)  (\(Int(dt * 1000))ms)")
        }
        board.place(mv.0, mv.1, side)
        if board.makesFive(mv.0, mv.1, side) { return side == 1 ? 1 : -1 }
        side = 3 - side
    }
    return 0
}

// ------------------------------ Elo ------------------------------

func eloDiff(_ p: Double) -> Double {
    let q = min(max(p, 1e-3), 1.0 - 1e-3)
    return -400.0 * log10(1.0 / q - 1.0)
}

func eloSE(_ p: Double, _ n: Int) -> Double {
    let q = min(max(p, 0.01), 0.99)
    return (400.0 / log(10.0)) * sqrt(1.0 / (Double(n) * q * (1.0 - q)))
}

// ------------------------------ 参数 ------------------------------

func argValue(_ args: [String], _ key: String) -> String? {
    guard let i = args.firstIndex(of: key), i + 1 < args.count else { return nil }
    return args[i + 1]
}

let args = CommandLine.arguments
let games = Int(argValue(args, "--games") ?? "100") ?? 100
let stmDepth = Int(argValue(args, "--depth") ?? "6") ?? 6
let stmTime = Int(argValue(args, "--time") ?? "1000") ?? 1000
let oppName = argValue(args, "--opponent") ?? "greedy"
let oppDepth = Int(argValue(args, "--opp-depth") ?? "4") ?? 4
let oppTime = Int(argValue(args, "--opp-time") ?? "300") ?? 300
let anchorElo = Double(argValue(args, "--anchor-elo") ?? "") ?? 0.0
let explicitPort = argValue(args, "--port")
let verbose = args.contains("--verbose")

// ------------------------------ 连接 ------------------------------

let port = SerialPort()
var path = explicitPort
if path == nil {
    if let dev = USBMatcher.find() { path = dev.path }
}
guard let devPath = path, port.open(devPath) else {
    print("未找到 Rapfi 引擎端口（可用 --port 指定）")
    exit(1)
}
print("端口: \(devPath)")
let stm = Stm32(port: port)
/* 连接后排空启动信息(READY 等)，避免读帧错位 */
_ = stm.readFrame(timeoutMs: 400)
stm.rx.removeAll()

// 对手
let opponent: Mover
let opponentIsStm: Bool
switch oppName {
case "self":
    opponent = StmMover(depth: oppDepth, ms: oppTime)
    opponentIsStm = true
default:
    guard let level = RefEngine.Level(rawValue: oppName) else {
        print("未知对手 \(oppName)（可选 random/greedy/ab2/ab4/ab6/self）")
        exit(1)
    }
    opponent = RefMover(engine: RefEngine(level: level))
    opponentIsStm = false
}
let stmMover = StmMover(depth: stmDepth, ms: stmTime)

print("STM32 : \(stmMover.label)")
print("对手  : \(opponent.label)")
print("局数  : \(games)（交替先后手）\n")

// ------------------------------ 对局循环 ------------------------------

var win = 0
var loss = 0
var draw = 0
var illegal = 0

for g in 0..<games {
    let stmIsBlack = (g % 2 == 0)
    let black: Mover = stmIsBlack ? stmMover : opponent
    let white: Mover = stmIsBlack ? opponent : stmMover
    let result = playGame(stm: stm, black: black, white: white)

    var outcome = ""
    if result == 0 {
        draw += 1
        outcome = "平/异常"
        if opponentIsStm { illegal += 1 }
    } else {
        let stmWon = (stmIsBlack && result == 1) || (!stmIsBlack && result == -1)
        if stmWon { win += 1; outcome = "胜" } else { loss += 1; outcome = "负" }
    }
    let color = stmIsBlack ? "黑" : "白"
    print(String(format: "[%3d/%3d] STM32 执%@  %@   (累计 %d胜 %d负 %d平)",
                 g + 1, games, color, outcome, win, loss, draw))
}

// ------------------------------ 结果 ------------------------------

let played = win + loss + draw
let score = played > 0 ? (Double(win) + 0.5 * Double(draw)) / Double(played) : 0.0
print("\n===== 结果 =====")
print("STM32 : \(stmMover.label)")
print("对手  : \(opponent.label)")
print(String(format: "战绩  : %d胜 %d负 %d平 / %d 局", win, loss, draw, played))

if win + loss + draw > 0 {
    print(String(format: "得分率: %.3f", score))
    if win > 0 || loss > 0 {
        let d = eloDiff(score)
        let se = eloSE(score, played)
        print(String(format: "Elo 差: %+.0f  ±%.0f (95%% CI)", d, 1.96 * se))
        if anchorElo != 0 {
            print(String(format: "若对手 %.0f 分，则 STM32 ≈ %.0f 分", anchorElo, anchorElo + d))
        }
    } else {
        print("（全部平局，无法估计 Elo）")
    }
}
port.close()

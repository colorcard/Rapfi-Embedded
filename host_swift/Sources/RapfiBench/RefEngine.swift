import Foundation

/// 内置参考对手（用于锚定/对比 STM32 棋力）。
final class RefEngine {
    enum Level: String, CaseIterable {
        case random
        case greedy
        case ab2
        case ab4
        case ab6
    }

    let level: Level
    private var nodes = 0
    private var nodeCap = 0

    init(level: Level) { self.level = level }

    func bestMove(_ board: Board, side: Int) -> (Int, Int)? {
        let cands = board.candidates(distance: 2)
        guard !cands.isEmpty else { return nil }

        // 1) 自己能成五
        for (x, y) in cands where board.makesFive(x, y, side) { return (x, y) }
        // 2) 挡对手成五
        let opp = 3 - side
        for (x, y) in cands where board.makesFive(x, y, opp) { return (x, y) }

        switch level {
        case .random:
            return cands.randomElement()
        case .greedy:
            return cands.max { a, b in
                value(board, a, side) < value(board, b, side)
            }
        case .ab2:
            return search(board, side, depth: 2, width: 12, cap: 120000)
        case .ab4:
            return search(board, side, depth: 4, width: 12, cap: 200000)
        case .ab6:
            return search(board, side, depth: 6, width: 12, cap: 400000)
        }
    }

    private func value(_ b: Board, _ p: (Int, Int), _ side: Int) -> Int {
        b.moveValue(p.0, p.1, side) * 2 + b.moveValue(p.0, p.1, 3 - side)
    }

    private func ordered(_ b: Board, _ side: Int, _ cands: [(Int, Int)]) -> [(Int, Int)] {
        cands.sorted { value(b, $0, side) > value(b, $1, side) }
    }

    private func search(_ board: Board, _ side: Int, depth: Int,
                        width: Int, cap: Int) -> (Int, Int)? {
        var b = board
        nodes = 0
        nodeCap = cap
        var alpha = -30001
        var best: (Int, Int)?
        var bestScore = -30001
        let cands = ordered(b, side, b.candidates(distance: 2))
        for p in cands.prefix(width) {
            b.place(p.0, p.1, side)
            let sc: Int
            if b.makesFive(p.0, p.1, side) {
                sc = 30000
            } else {
                sc = -negamax(&b, 3 - side, depth - 1, -30001, -alpha)
            }
            b.undo()
            if sc > bestScore {
                bestScore = sc
                best = p
            }
            if sc > alpha { alpha = sc }
            if nodes >= nodeCap { break }
        }
        return best ?? cands.first
    }

    private func negamax(_ b: inout Board, _ side: Int, _ depth: Int,
                         _ alpha: Int, _ beta: Int) -> Int {
        nodes += 1
        if depth <= 0 || nodes >= nodeCap {
            return b.eval(side) - b.eval(3 - side)
        }
        let cands = ordered(b, side, b.candidates(distance: 1))
        if cands.isEmpty { return 0 }
        var a = alpha
        var best = -30001
        for p in cands.prefix(10) {
            b.place(p.0, p.1, side)
            let sc: Int
            if b.makesFive(p.0, p.1, side) {
                sc = 30000
            } else {
                sc = -negamax(&b, 3 - side, depth - 1, -beta, -a)
            }
            b.undo()
            if sc > best { best = sc }
            if sc > a { a = sc }
            if a >= beta { break }
            if nodes >= nodeCap { break }
        }
        return best
    }
}

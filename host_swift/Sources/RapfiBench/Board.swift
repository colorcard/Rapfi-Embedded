import Foundation

/// 15 路棋盘 + 五连判定 + 5 连窗口评估（供参考引擎使用）。
struct Board {
    static let n = 15
    static let cells = n * n
    static let score: [Int] = [0, 1, 8, 60, 500, 30000]

    var cell = [Int](repeating: 0, count: Board.cells)
    private(set) var history: [Int] = []

    static let windows: [[Int]] = {
        var out: [[Int]] = []
        let dirs = [(1, 0), (0, 1), (1, 1), (1, -1)]
        for (dx, dy) in dirs {
            for y in 0..<n {
                for x in 0..<n {
                    let ex = x + dx * 4, ey = y + dy * 4
                    guard ex >= 0, ex < n, ey >= 0, ey < n else { continue }
                    out.append((0..<5).map { k in (y + dy * k) * n + (x + dx * k) })
                }
            }
        }
        return out
    }()

    func at(_ x: Int, _ y: Int) -> Int { cell[y * Board.n + x] }

    subscript(_ x: Int, _ y: Int) -> Int { cell[y * Board.n + x] }

    mutating func place(_ x: Int, _ y: Int, _ s: Int) {
        let i = y * Board.n + x
        cell[i] = s
        history.append(i)
    }

    mutating func undo() {
        if let i = history.popLast() { cell[i] = 0 }
    }

    func inBounds(_ x: Int, _ y: Int) -> Bool {
        x >= 0 && x < Board.n && y >= 0 && y < Board.n
    }

    /// 在 (x,y) 落 s 是否成五（不修改棋盘）。
    func makesFive(_ x: Int, _ y: Int, _ s: Int) -> Bool {
        let dirs = [(1, 0), (0, 1), (1, 1), (1, -1)]
        for (dx, dy) in dirs {
            var count = 1
            for sgn in [-1, 1] {
                var cx = x + dx * sgn, cy = y + dy * sgn
                while inBounds(cx, cy) && at(cx, cy) == s {
                    count += 1
                    cx += dx * sgn
                    cy += dy * sgn
                }
            }
            if count >= 5 { return true }
        }
        return false
    }

    /// 全盘是否有胜者（扫描最近若干手，简单起见全扫）。
    func winner() -> Int {
        for i in 0..<Board.cells where cell[i] != 0 {
            let x = i % Board.n, y = i / Board.n
            if makesFive(x, y, cell[i]) { return cell[i] }
        }
        return 0
    }

    /// 邻近（2 格内）有子的空点。
    func candidates(distance: Int = 2) -> [(Int, Int)] {
        var seen = [Bool](repeating: false, count: Board.cells)
        var out: [(Int, Int)] = []
        for i in 0..<Board.cells where cell[i] != 0 {
            let x = i % Board.n, y = i / Board.n
            for dy in -distance...distance {
                for dx in -distance...distance {
                    let nx = x + dx, ny = y + dy
                    guard inBounds(nx, ny) else { continue }
                    let j = ny * Board.n + nx
                    if cell[j] == 0 && !seen[j] {
                        seen[j] = true
                        out.append((nx, ny))
                    }
                }
            }
        }
        if out.isEmpty { out.append((Board.n / 2, Board.n / 2)) }
        return out
    }

    /// 窗口求和评估（本方视角，越大越好）。
    func eval(_ side: Int) -> Int {
        let opp = 3 - side
        var total = 0
        for w in Board.windows {
            var mine = 0
            var bad = false
            for c in w {
                let v = cell[c]
                if v == side { mine += 1 } else if v == opp { bad = true; break }
            }
            if !bad { total += Board.score[mine] }
        }
        return total
    }

    /// 某一手对某方的局部价值（用于排序）。
    func moveValue(_ x: Int, _ y: Int, _ side: Int) -> Int {
        let i = y * Board.n + x
        var total = 0
        for w in Board.windows where w.contains(i) {
            var mine = 1
            var bad = false
            for c in w {
                if c == i { continue }
                let v = cell[c]
                if v == side { mine += 1 } else if v != 0 { bad = true; break }
            }
            if !bad { total += Board.score[mine] }
        }
        return total
    }
}

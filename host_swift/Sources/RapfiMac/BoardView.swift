import SwiftUI

/// 棋盘：木色底 + 网格 + 立体棋子，支持点按落子与悬停预览。
struct BoardView: View {
    @ObservedObject var game: GameViewModel
    @State private var hoverCell: Move?

    private let n = GameViewModel.n
    private let pad: CGFloat = 34

    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)
            let step = (side - pad * 2) / CGFloat(n - 1)
            let ox = (geo.size.width - side) / 2
            let oy = (geo.size.height - side) / 2

            ZStack {
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .fill(LinearGradient(colors: [Color(red: 0.93, green: 0.79, blue: 0.55),
                                                  Color(red: 0.87, green: 0.70, blue: 0.44)],
                                         startPoint: .topLeading, endPoint: .bottomTrailing))
                    .overlay(RoundedRectangle(cornerRadius: 18, style: .continuous)
                        .strokeBorder(.black.opacity(0.12), lineWidth: 1))
                    .shadow(color: .black.opacity(0.18), radius: 10, y: 4)

                Canvas { context, size in
                    var ctx = context
                    draw(ctx: &ctx, size: size, side: side, step: step, ox: ox, oy: oy)
                }
                .contentShape(Rectangle())
                .gesture(DragGesture(minimumDistance: 0).onEnded { value in
                    if let m = hit(value.location, side: side, step: step, ox: ox, oy: oy) {
                        game.click(x: m.x, y: m.y)
                    }
                })
                .onContinuousHover { phase in
                    switch phase {
                    case .active(let p): hoverCell = hit(p, side: side, step: step, ox: ox, oy: oy)
                    case .ended: hoverCell = nil
                    }
                }
            }
        }
        .aspectRatio(1, contentMode: .fit)
    }

    // MARK: - 几何

    private func point(_ x: Int, _ y: Int, side: CGFloat, step: CGFloat,
                       ox: CGFloat, oy: CGFloat) -> CGPoint {
        CGPoint(x: ox + pad + CGFloat(x) * step,
                y: oy + pad + CGFloat(n - 1 - y) * step)
    }

    private func hit(_ p: CGPoint, side: CGFloat, step: CGFloat,
                     ox: CGFloat, oy: CGFloat) -> Move? {
        let gx = (p.x - ox - pad) / step
        let gy = CGFloat(n - 1) - (p.y - oy - pad) / step
        let x = Int(gx.rounded())
        let y = Int(gy.rounded())
        guard (0..<n).contains(x), (0..<n).contains(y) else { return nil }
        // 距最近交叉点过远则忽略
        let target = point(x, y, side: side, step: step, ox: ox, oy: oy)
        guard hypot(p.x - target.x, p.y - target.y) < step * 0.9 else { return nil }
        return Move(x: x, y: y)
    }

    // MARK: - 绘制

    private func draw(ctx: inout GraphicsContext, size: CGSize, side: CGFloat,
                      step: CGFloat, ox: CGFloat, oy: CGFloat) {
        let line = Color(red: 0.30, green: 0.21, blue: 0.12).opacity(0.75)

        for i in 0..<n {
            var h = Path()
            h.move(to: point(0, i, side: side, step: step, ox: ox, oy: oy))
            h.addLine(to: point(n - 1, i, side: side, step: step, ox: ox, oy: oy))
            ctx.stroke(h, with: .color(line), lineWidth: 1)

            var v = Path()
            v.move(to: point(i, 0, side: side, step: step, ox: ox, oy: oy))
            v.addLine(to: point(i, n - 1, side: side, step: step, ox: ox, oy: oy))
            ctx.stroke(v, with: .color(line), lineWidth: 1)
        }

        for (sx, sy) in [(3, 3), (11, 3), (3, 11), (11, 11), (7, 7)] {
            let c = point(sx, sy, side: side, step: step, ox: ox, oy: oy)
            let r: CGFloat = 2.6
            ctx.fill(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: r * 2, height: r * 2)),
                     with: .color(Color(red: 0.25, green: 0.17, blue: 0.09).opacity(0.9)))
        }

        let radius = step * 0.44
        for y in 0..<n {
            for x in 0..<n where game.board[y][x] != 0 {
                let c = point(x, y, side: side, step: step, ox: ox, oy: oy)
                stone(&ctx, center: c, radius: radius, black: game.board[y][x] == Side.black.rawValue)
            }
        }

        if let m = game.lastMove, game.status == .playing {
            let c = point(m.x, m.y, side: side, step: step, ox: ox, oy: oy)
            let color: Color = game.board[m.y][m.x] == Side.black.rawValue
                ? Color(red: 1, green: 0.45, blue: 0.35) : Color(red: 0.95, green: 0.25, blue: 0.2)
            let r: CGFloat = 3.2
            ctx.fill(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: r * 2, height: r * 2)),
                     with: .color(color))
        }

        if let h = hoverCell, game.status == .playing, game.turn == game.human,
           game.board[h.y][h.x] == 0 {
            let c = point(h.x, h.y, side: side, step: step, ox: ox, oy: oy)
            var ghost = ctx
            ghost.opacity = 0.35
            stone(&ghost, center: c, radius: radius, black: game.human == .black)
        }

        // 坐标
        let labelColor = Color(red: 0.32, green: 0.23, blue: 0.13).opacity(0.9)
        for i in 0..<n {
            let colName = String(UnicodeScalar(UInt8(65 + i)))
            let bottom = point(i, 0, side: side, step: step, ox: ox, oy: oy)
            ctx.draw(Text(colName).font(.system(size: 10, weight: .medium))
                        .foregroundColor(labelColor),
                     at: CGPoint(x: bottom.x, y: bottom.y + 16))
            let left = point(0, i, side: side, step: step, ox: ox, oy: oy)
            ctx.draw(Text("\(i + 1)").font(.system(size: 10, weight: .medium))
                        .foregroundColor(labelColor),
                     at: CGPoint(x: left.x - 18, y: left.y))
        }
    }

    private func stone(_ ctx: inout GraphicsContext, center: CGPoint, radius r: CGFloat,
                       black: Bool) {
        let rect = CGRect(x: center.x - r, y: center.y - r, width: r * 2, height: r * 2)
        let shadow = ctx
        shadow.fill(Path(ellipseIn: rect.offsetBy(dx: 0, dy: 1.4)),
                    with: .color(.black.opacity(0.30)))
        let colors: [Color] = black
            ? [Color(white: 0.42), Color(white: 0.10)]
            : [Color.white, Color(white: 0.74)]
        let shading = GraphicsContext.Shading.radialGradient(
            Gradient(colors: colors),
            center: CGPoint(x: center.x - r * 0.32, y: center.y - r * 0.34),
            startRadius: r * 0.06, endRadius: r * 1.15)
        ctx.fill(Path(ellipseIn: rect), with: shading)
        ctx.stroke(Path(ellipseIn: rect), with: .color(.black.opacity(black ? 0.25 : 0.18)),
                   lineWidth: 0.8)
    }
}

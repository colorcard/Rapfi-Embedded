import AppKit
import Charts
import SwiftUI

struct ContentView: View {
    @StateObject private var game = GameViewModel()

    var body: some View {
        NavigationSplitView {
            SidebarView(game: game)
                .navigationSplitViewColumnWidth(min: 280, ideal: 320, max: 380)
        } detail: {
            ZStack {
                Color(nsColor: .windowBackgroundColor)
                VStack(spacing: 10) {
                    BoardView(game: game).padding(.horizontal, 26).padding(.top, 18)
                    AnalysisPanel(game: game).padding(.horizontal, 26)
                    EvalChart(points: game.evalPoints).padding(.horizontal, 26).padding(.bottom, 14)
                }
                resultOverlay
            }
            .frame(minWidth: 600, minHeight: 720)
            .navigationTitle("Rapfi · 五子棋")
        }
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button { game.newGame() } label: {
                    Label("新局", systemImage: "plus.circle")
                }
                .keyboardShortcut("n")
                .help("新局 (⌘N)")

                Button { game.undo() } label: {
                    Label("悔棋", systemImage: "arrow.uturn.backward")
                }
                .keyboardShortcut("z")
                .disabled(game.moves.isEmpty || game.pendingLock)
                .help("悔棋 (⌘Z)")

                Button { game.swapSides() } label: {
                    Label("换先", systemImage: "arrow.left.arrow.right")
                }
                .help("交换先后手")
            }
        }
        .frame(minWidth: 960, minHeight: 780)
        .task {
            game.autoConnect()
            game.startMonitoring()
        }
    }

    @ViewBuilder
    private var resultOverlay: some View {
        if game.status != .playing {
            VStack(spacing: 16) {
                Image(systemName: resultIcon)
                    .font(.system(size: 44, weight: .semibold))
                    .foregroundStyle(resultTint)
                Text(resultTitle).font(.title2.weight(.bold))
                Button {
                    game.newGame()
                } label: {
                    Text("再来一局").frame(minWidth: 120)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .keyboardShortcut(.return)
            }
            .padding(30)
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous)
                .strokeBorder(.primary.opacity(0.08)))
            .shadow(color: .black.opacity(0.22), radius: 24, y: 10)
            .transition(.scale.combined(with: .opacity))
        }
    }

    private var resultTitle: String {
        switch game.status {
        case .humanWin: return "你赢了"
        case .engineWin: return "引擎获胜"
        case .draw: return "平局"
        case .playing: return ""
        }
    }

    private var resultIcon: String {
        switch game.status {
        case .humanWin: return "checkmark.circle.fill"
        case .engineWin: return "cpu"
        case .draw: return "equal.circle.fill"
        case .playing: return "circle"
        }
    }

    private var resultTint: Color {
        switch game.status {
        case .humanWin: return .green
        case .engineWin: return .red
        case .draw: return .secondary
        case .playing: return .secondary
        }
    }
}

// MARK: - 形势分析曲线

struct EvalChart: View {
    let points: [EvalPoint]

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("形势分析").font(.headline)
                Text("白方视角，上=白优 / 下=黑优").font(.caption).foregroundStyle(.secondary)
                Spacer()
            }
            if points.isEmpty {
                RoundedRectangle(cornerRadius: 12)
                    .fill(.quaternary)
                    .frame(height: 120)
                    .overlay(Text("暂无分析数据").font(.caption).foregroundStyle(.secondary))
            } else {
                Chart {
                    RuleMark(y: .value("均势", 0))
                        .foregroundStyle(.secondary.opacity(0.7))
                        .lineStyle(StrokeStyle(lineWidth: 1, dash: [4, 4]))
                    ForEach(points) { p in
                        AreaMark(x: .value("手数", p.move), y: .value("形势", p.value))
                            .interpolationMethod(.monotone)
                            .foregroundStyle(LinearGradient(
                                colors: [.accentColor.opacity(0.35), .accentColor.opacity(0.02)],
                                startPoint: .top, endPoint: .bottom))
                        LineMark(x: .value("手数", p.move), y: .value("形势", p.value))
                            .interpolationMethod(.monotone)
                            .foregroundStyle(Color.accentColor)
                    }
                }
                .chartYScale(domain: -2000...2000)
                .chartXAxisLabel("手数")
                .frame(height: 132)
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(.background.secondary))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .strokeBorder(.primary.opacity(0.08)))
            }
        }
    }
}

// MARK: - 侧边信息栏

struct SidebarView: View {
    @ObservedObject var game: GameViewModel

    private var subtitle: String {
        if game.connected {
            if !game.deviceName.isEmpty {
                return "\(game.deviceName) · \(game.portPath)"
            }
            return game.portPath
        }
        return game.errorText ?? "通过 Type-C 连接 STM32"
    }

    var body: some View {
        List {
            Section("连接") {
                HStack(spacing: 10) {
                    Circle()
                        .fill(game.connected ? Color.green
                              : (game.connecting ? Color.orange : Color.red))
                        .frame(width: 9, height: 9)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(game.connected ? "已连接"
                             : (game.connecting ? "正在查找设备…" : "未连接"))
                            .font(.callout.weight(.medium))
                        Text(subtitle).font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(2)
                    }
                    Spacer()
                    if !game.connected {
                        Button("连接") { game.autoConnect() }
                            .buttonStyle(.bordered)
                            .controlSize(.small)
                    }
                }
            }

            Section("对局") {
                playerRow(side: game.human, tag: "你")
                playerRow(side: game.engineSide, tag: "引擎")
                if game.status == .playing {
                    HStack {
                        Text("轮到").foregroundStyle(.secondary)
                        Spacer()
                        Text(game.turn == game.human ? "你" : "引擎")
                            .fontWeight(.semibold)
                            .foregroundStyle(game.turn == game.human ? Color.green : Color.accentColor)
                    }
                    .font(.callout)
                }
            }

            Section("引擎") {
                Stepper(value: Binding(
                    get: { game.depth },
                    set: { game.depth = max(1, min(12, $0)) }), in: 1...12) {
                    HStack {
                        Text("搜索深度")
                        Spacer()
                        Text("\(game.depth)").monospacedDigit().foregroundStyle(.secondary)
                    }
                }
                if game.thinking {
                    HStack(spacing: 8) {
                        ProgressView().controlSize(.small)
                        Text("思考中…").foregroundStyle(.secondary)
                    }
                } else if let info = game.info {
                    statRow("完成深度", "\(info.depth)")
                    statRow("形势评分", "\(info.score)")
                    statRow("搜索节点", "\(info.nodes)")
                    statRow("耗时", "\(info.ms) ms")
                } else {
                    Text("尚未分析").font(.caption).foregroundStyle(.tertiary)
                }
            }

            Section("着法记录") {
                if game.moves.isEmpty {
                    Text("—").font(.caption).foregroundStyle(.tertiary)
                } else {
                    ScrollView {
                        VStack(alignment: .leading, spacing: 2) {
                            ForEach(game.moves.reversed()) { m in
                                HStack(spacing: 8) {
                                    Text("\(m.index).").monospacedDigit()
                                        .foregroundStyle(.secondary)
                                        .frame(width: 26, alignment: .trailing)
                                    Text(m.side == .black ? "●" : "○")
                                    Text(m.coord).monospacedDigit()
                                    Spacer()
                                    if let s = m.score {
                                        Text("\(s)").monospacedDigit()
                                            .foregroundStyle(.secondary)
                                    }
                                }
                                .font(.caption)
                            }
                        }
                    }
                    .frame(maxHeight: 170)
                }
                Button {
                    game.exportLog()
                } label: {
                    Label("导出日志", systemImage: "square.and.arrow.down")
                }
                .disabled(game.moves.isEmpty)
                if let p = game.exportedPath {
                    Text(p).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                }
            }

            Section {
                Button {
                    game.newGame()
                } label: { Label("新局", systemImage: "plus.circle") }
                Button {
                    game.undo()
                } label: { Label("悔棋", systemImage: "arrow.uturn.backward") }
                    .disabled(game.moves.isEmpty || game.pendingLock)
                Button {
                    game.swapSides()
                } label: { Label("交换先后手", systemImage: "arrow.left.arrow.right") }
            }
        }
        .listStyle(.sidebar)
    }

    private func playerRow(side: Side, tag: String) -> some View {
        HStack(spacing: 10) {
            StoneSwatch(black: side == .black)
            Text(tag)
            Spacer()
            if game.status == .playing && game.turn == side {
                Image(systemName: "arrowtriangle.right.fill")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
        }
    }

    private func statRow(_ label: String, _ value: String) -> some View {
        HStack {
            Text(label).foregroundStyle(.secondary)
            Spacer()
            Text(value).monospacedDigit()
        }
        .font(.callout)
    }
}

struct StoneSwatch: View {
    let black: Bool
    var size: CGFloat = 16

    var body: some View {
        Circle()
            .fill(RadialGradient(
                colors: black ? [Color(white: 0.45), Color(white: 0.08)]
                              : [.white, Color(white: 0.72)],
                center: .init(x: 0.35, y: 0.3), startRadius: 1, endRadius: size))
            .overlay(Circle().strokeBorder(.black.opacity(0.2), lineWidth: 0.6))
            .frame(width: size, height: size)
    }
}


// MARK: - 分析面板（指标 / 路线 / 局面代码）

struct AnalysisPanel: View {
    @ObservedObject var game: GameViewModel

    private var timeText: String {
        guard let i = game.info else { return "-" }
        if i.ms >= 1000 {
            return String(format: "%.1fs", Double(i.ms) / 1000.0)
        }
        return "\(i.ms)ms"
    }

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 0) {
                metric("深度", game.info.map { "\($0.depth)" } ?? "-")
                divider
                metric("评估", game.info.map { "\($0.score)" } ?? "-")
                divider
                metric("速度", "\(game.speed)")
                divider
                metric("节点数", game.info.map { "\($0.nodes)" } ?? "0")
                divider
                metric("用时", timeText)
            }
            .padding(.vertical, 6)
            .background(RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(.background.secondary))
            .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous)
                .strokeBorder(.primary.opacity(0.08)))

            infoRow("路线", game.pvText)
            infoRow("局面代码", game.positionCode)
        }
    }

    private var divider: some View {
        Divider().frame(height: 30)
    }

    private func metric(_ label: String, _ value: String) -> some View {
        VStack(spacing: 2) {
            Text(label).font(.caption).foregroundStyle(.secondary)
            Text(value).font(.system(.body, design: .rounded).weight(.semibold))
                .monospacedDigit()
        }
        .frame(maxWidth: .infinity)
    }

    private func infoRow(_ label: String, _ value: String) -> some View {
        HStack(spacing: 8) {
            Text(label).font(.callout.weight(.medium))
            Text(value.isEmpty ? "—" : value)
                .font(.system(.caption, design: .monospaced))
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .truncationMode(.middle)
            Spacer()
            if !value.isEmpty {
                Button {
                    let pb = NSPasteboard.general
                    pb.clearContents()
                    pb.setString(value, forType: .string)
                } label: {
                    Image(systemName: "doc.on.doc")
                }
                .buttonStyle(.borderless)
                .help("复制")
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(RoundedRectangle(cornerRadius: 8, style: .continuous)
            .fill(.background.secondary))
        .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous)
            .strokeBorder(.primary.opacity(0.08)))
    }
}

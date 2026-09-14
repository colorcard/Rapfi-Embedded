// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "RapfiEmbedded",
    platforms: [.macOS(.v14)],
    targets: [
        // 共享：串口 + USB 识别
        .target(
            name: "RapfiKit",
            path: "Sources/RapfiKit",
            linkerSettings: [.linkedFramework("IOKit")]
        ),
        // 原生 macOS 对弈界面
        .executableTarget(
            name: "RapfiMac",
            dependencies: ["RapfiKit"],
            path: "Sources/RapfiMac"
        ),
        // 无界面棋力评测（自对弈 / 对参考引擎 + Elo）
        .executableTarget(
            name: "rapfi-bench",
            dependencies: ["RapfiKit"],
            path: "Sources/RapfiBench"
        ),
    ]
)

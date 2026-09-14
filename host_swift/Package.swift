// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "RapfiMac",
    platforms: [.macOS(.v14)],
    targets: [
        .executableTarget(
            name: "RapfiMac",
            path: "Sources/RapfiMac",
            linkerSettings: [.linkedFramework("IOKit")]
        )
    ]
)

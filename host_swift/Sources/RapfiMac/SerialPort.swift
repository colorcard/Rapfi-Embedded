import Foundation
import Darwin

/// 通过 POSIX termios 访问 /dev/cu.usbmodem* 的串口封装。
final class SerialPort {
    private var fd: Int32 = -1
    private var running = false
    private var thread: Thread?
    private var buffer = Data()
    private let lock = NSLock()

    /// 收到一行（已去除 \\r 与提示符 "> "），在主线程回调。
    var onLine: ((String) -> Void)?

    deinit { close() }

    /// 列出候选端口。
    static func candidates() -> [String] {
        let fm = FileManager.default
        let names = (try? fm.contentsOfDirectory(atPath: "/dev")) ?? []
        return names
            .filter { $0.hasPrefix("cu.usbmodem") }
            .map { "/dev/" + $0 }
            .sorted()
    }

    /// 探测某端口是否是 STM32 引擎（发 TURN 看是否回 TURN）。
    static func probe(_ path: String) -> Bool {
        let p = SerialPort()
        guard p.open(path) else { return false }
        p.writeLine("TURN")
        let deadline = Date().addingTimeInterval(0.5)
        var seen = ""
        while Date() < deadline {
            if let ln = p.pollLine(timeoutMs: 80) {
                seen += ln
                if seen.contains("TURN") { break }
            }
        }
        p.close()
        return seen.contains("TURN")
    }

    func open(_ path: String) -> Bool {
        close()
        fd = Darwin.open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)
        if fd < 0 { return false }
        var tty = termios()
        if tcgetattr(fd, &tty) != 0 { close(); return false }
        cfmakeraw(&tty)
        cfsetispeed(&tty, speed_t(B115200))
        cfsetospeed(&tty, speed_t(B115200))
        if tcsetattr(fd, TCSANOW, &tty) != 0 { close(); return false }
        return true
    }

    func close() {
        running = false
        if let t = thread { t.cancel(); thread = nil }
        if fd >= 0 { Darwin.close(fd); fd = -1 }
        lock.lock(); buffer.removeAll(); lock.unlock()
    }

    func writeLine(_ s: String) {
        guard fd >= 0 else { return }
        let data = Data((s + "\n").utf8)
        data.withUnsafeBytes { raw in
            if let base = raw.baseAddress {
                _ = Darwin.write(fd, base, raw.count)
            }
        }
    }

    /// 读取并切出完整行（阻塞至多 timeoutMs）。给探测用。
    private func pollLine(timeoutMs: Int) -> String? {
        let deadline = Date().addingTimeInterval(Double(timeoutMs) / 1000.0)
        while Date() < deadline {
            if let line = takeLine() { return line }
            var b = [UInt8](repeating: 0, count: 256)
            let n = b.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress, $0.count) }
            if n > 0 {
                lock.lock(); buffer.append(contentsOf: b[0..<n]); lock.unlock()
            } else {
                usleep(5000)
            }
        }
        return takeLine()
    }

    private func takeLine() -> String? {
        lock.lock()
        defer { lock.unlock() }
        guard let idx = buffer.firstIndex(of: 0x0A) else { return nil }
        let slice = buffer[buffer.startIndex..<idx]
        buffer.removeSubrange(buffer.startIndex...idx)
        var s = String(data: Data(slice), encoding: .utf8) ?? ""
        s = s.replacingOccurrences(of: "\r", with: "")
        s = s.trimmingCharacters(in: .whitespaces)
        if s.hasPrefix(">") {
            s.removeFirst()
            s = s.trimmingCharacters(in: .whitespaces)
        }
        return s.isEmpty ? nil : s
    }

    /// 启动后台读线程。
    func startReading() {
        running = true
        let t = Thread { [weak self] in self?.readLoop() }
        t.stackSize = 256 * 1024
        thread = t
        t.start()
    }

    private func readLoop() {
        var b = [UInt8](repeating: 0, count: 1024)
        while running {
            let n = b.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress, $0.count) }
            if n > 0 {
                lock.lock(); buffer.append(contentsOf: b[0..<n]); lock.unlock()
                drainLines()
            } else if n < 0 {
                if errno == EAGAIN || errno == EWOULDBLOCK { usleep(2000); continue }
                break
            } else {
                break
            }
        }
    }

    private func drainLines() {
        while let line = takeLine() {
            DispatchQueue.main.async { [weak self] in self?.onLine?(line) }
        }
    }
}

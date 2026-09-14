import Foundation
import Darwin

/// 通过 POSIX termios 访问 /dev/cu.usbmodem* 的串口封装（原始字节流）。
final class SerialPort {
    private var fd: Int32 = -1
    private var running = false
    private var thread: Thread?
    private var buffer = Data()
    private let lock = NSLock()

    /// 收到一批原始字节（在主线程回调）。
    var onBytes: ((Data) -> Void)?

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

    /// 探测某端口是否是 Rapfi 引擎（发 PING，看是否回 READY 0x85）。
    static func probe(_ path: String) -> Bool {
        let p = SerialPort()
        guard p.open(path) else { return false }
        p.write(Data([0x07, 0x00, 0x00, 0x00]))
        let deadline = Date().addingTimeInterval(0.5)
        var found = false
        while Date() < deadline {
            let chunk = p.readBytes(timeoutMs: 60)
            if chunk.contains(0x85) { found = true; break }
        }
        p.close()
        return found
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

    func write(_ data: Data) {
        guard fd >= 0 else { return }
        data.withUnsafeBytes { raw in
            if let base = raw.baseAddress {
                _ = Darwin.write(fd, base, raw.count)
            }
        }
    }

    /// 同步读取一段（给探测用）。
    private func readBytes(timeoutMs: Int) -> Data {
        let deadline = Date().addingTimeInterval(Double(timeoutMs) / 1000.0)
        while Date() < deadline {
            var b = [UInt8](repeating: 0, count: 64)
            let n = b.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress, $0.count) }
            if n > 0 { return Data(b[0..<n]) }
            usleep(4000)
        }
        return Data()
    }

    func startReading() {
        running = true
        let t = Thread { [weak self] in self?.readLoop() }
        t.stackSize = 256 * 1024
        thread = t
        t.start()
    }

    private func readLoop() {
        var b = [UInt8](repeating: 0, count: 512)
        while running {
            let n = b.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress, $0.count) }
            if n > 0 {
                lock.lock(); buffer.append(contentsOf: b[0..<n]); lock.unlock()
                drain()
            } else if n < 0 {
                if errno == EAGAIN || errno == EWOULDBLOCK { usleep(2000); continue }
                break
            } else {
                break
            }
        }
    }

    private func drain() {
        lock.lock()
        let out = buffer
        buffer.removeAll()
        lock.unlock()
        guard !out.isEmpty else { return }
        DispatchQueue.main.async { [weak self] in self?.onBytes?(out) }
    }
}

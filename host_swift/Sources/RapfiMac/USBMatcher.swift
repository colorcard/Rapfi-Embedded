import Foundation
import IOKit

/// 通过 IOKit 按 USB VID:PID 识别设备的串口节点。
///
/// 比“打开每个串口发 TURN 试探”更稳：不打扰其它设备、不做 I/O，
/// 并能拿到 USB 产品名/序列号。
enum USBMatcher {
    struct Device {
        let path: String
        let product: String
        let serial: String
        let vendor: Int
        let pid: Int
    }

    /// 查找匹配 VID:PID 的 CDC 串口。默认 STM32 虚拟串口 0x0483:0x5740。
    static func find(vendor wantVid: Int = 0x0483, pid wantPid: Int = 0x5740) -> Device? {
        var iterator: io_iterator_t = 0
        guard let matching = IOServiceMatching("IOSerialBSDClient"),
              IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS
        else { return nil }
        defer { IOObjectRelease(iterator) }

        var service = IOIteratorNext(iterator)
        while service != 0 {
            let path = devicePath(service)
            if let path, let usb = usbInfo(from: service),
               usb.vid == wantVid, usb.pid == wantPid {
                IOObjectRelease(service)
                return Device(path: path, product: usb.product, serial: usb.serial,
                              vendor: usb.vid, pid: usb.pid)
            }
            IOObjectRelease(service)
            service = IOIteratorNext(iterator)
        }
        return nil
    }

    /// 列出所有 STM32 串口（可能多个）。
    static func findAll(vendor wantVid: Int = 0x0483, pid wantPid: Int = 0x5740) -> [Device] {
        var result: [Device] = []
        var iterator: io_iterator_t = 0
        guard let matching = IOServiceMatching("IOSerialBSDClient"),
              IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS
        else { return [] }
        defer { IOObjectRelease(iterator) }
        var service = IOIteratorNext(iterator)
        while service != 0 {
            if let path = devicePath(service),
               let usb = usbInfo(from: service),
               usb.vid == wantVid, usb.pid == wantPid {
                result.append(Device(path: path, product: usb.product, serial: usb.serial,
                                     vendor: usb.vid, pid: usb.pid))
            }
            IOObjectRelease(service)
            service = IOIteratorNext(iterator)
        }
        return result
    }

    // MARK: - IOKit 辅助

    private struct USBInfo {
        let vid: Int
        let pid: Int
        let product: String
        let serial: String
    }

    /// 串口节点路径：优先 callout（/dev/cu.*），回退 dialin（/dev/tty.*）。
    private static func devicePath(_ service: io_registry_entry_t) -> String? {
        stringProperty(service, "IOCalloutDevice")
            ?? stringProperty(service, "IODialinDevice")
    }

    /// 沿父节点上溯，收集 idVendor/idProduct 与 USB 产品名/序列号。
    private static func usbInfo(from entry: io_registry_entry_t) -> USBInfo? {
        var vid: Int?
        var pid: Int?
        var product = ""
        var serial = ""
        var current = entry
        IOObjectRetain(current)
        var depth = 0
        while depth < 10 {
            if vid == nil, let v = intProperty(current, "idVendor"),
               let p = intProperty(current, "idProduct") {
                vid = v
                pid = p
            }
            if product.isEmpty, let s = stringProperty(current, "USB Product Name") {
                product = s
            }
            if serial.isEmpty, let s = stringProperty(current, "USB Serial Number") {
                serial = s
            }
            if vid != nil, !product.isEmpty {
                break
            }
            var parent: io_registry_entry_t = 0
            let kr = IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent)
            IOObjectRelease(current)
            guard kr == KERN_SUCCESS else {
                current = 0
                break
            }
            current = parent
            depth += 1
        }
        if current != 0 { IOObjectRelease(current) }
        guard let v = vid, let p = pid else { return nil }
        return USBInfo(vid: v, pid: p, product: product, serial: serial)
    }

    private static func stringProperty(_ entry: io_registry_entry_t, _ key: String) -> String? {
        guard let cf = IORegistryEntryCreateCFProperty(entry, key as CFString,
                                                       kCFAllocatorDefault, 0) else { return nil }
        return cf.takeRetainedValue() as? String
    }

    private static func intProperty(_ entry: io_registry_entry_t, _ key: String) -> Int? {
        guard let cf = IORegistryEntryCreateCFProperty(entry, key as CFString,
                                                       kCFAllocatorDefault, 0) else { return nil }
        let value = cf.takeRetainedValue()
        return (value as? NSNumber)?.intValue
    }
}

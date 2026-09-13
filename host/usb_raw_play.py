#!/usr/bin/env python3
"""把裸 RGB332 帧文件以“变化条带(delta)”方式推流到 STM32G474 + ST7789。

协议（固件侧解析）：
    b'A5' + fmt(1B) + mask(2B, 小端, bit i=条带 i 有变化) + 各变化条带的源数据
条带 = 16 输出行；每条带源数据 = src_w * (16/scale) * bpp 字节。
静止条带既不发也不写 → 省带宽、缩短刷屏/撕裂窗口。

依赖：pyserial、numpy

用法：
    python3 host/usb_raw_play.py out.raw
    python3 host/usb_raw_play.py out.raw --fps 5 --loop            # delta
    python3 host/usb_raw_play.py out.raw --full                    # 每帧整帧
"""
import argparse
import struct
import sys
import time

MAGIC0 = 0xA5
STRIP_ROWS = 16


def main() -> int:
    ap = argparse.ArgumentParser(description="Raw RGB332 delta player over USB CDC")
    ap.add_argument("file")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341")
    ap.add_argument("--fps", type=float, default=0.0, help="0=用文件帧率")
    ap.add_argument("--loop", action="store_true")
    ap.add_argument("--full", action="store_true", help="禁用 delta，每帧整帧发送")
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
        import numpy as np  # noqa: WPS433
    except ImportError as exc:
        print(f"缺少依赖：{exc}", file=sys.stderr)
        return 1

    with open(args.file, "rb") as fp:
        head = fp.read(16)
        if len(head) < 16 or head[:4] != b"R332":
            print("不是有效的 R332 文件", file=sys.stderr)
            return 1
        fmt_byte, w, h, file_fps = struct.unpack("<BHHH", head[4:11])
        frame_bytes = w * h
        data = fp.read()

    total = len(data) // frame_bytes
    fps = args.fps if args.fps > 0 else float(file_fps)
    period = 1.0 / fps if fps > 0 else 0.0
    magic = bytes([MAGIC0, fmt_byte])
    # 输出条带恒为 15（240/16）；每条带的源字节数 = w * (16/scale)（RGB332 bpp=1）
    scale = 1 if fmt_byte == 0x5B else 2
    n_strips = 240 // STRIP_ROWS
    src_rows_per_strip = STRIP_ROWS // scale
    strip_bytes = w * src_rows_per_strip
    assert n_strips * strip_bytes == frame_bytes, (n_strips, strip_bytes, frame_bytes)

    ser = serial.Serial(args.port, 115200, timeout=1, write_timeout=10)
    ser.dtr = True
    ser.rts = True

    import queue as _queue
    import threading
    wq = _queue.Queue(maxsize=8)

    def _writer():
        while True:
            item = wq.get()
            if item is None:
                break
            ser.write(item)

    wt = threading.Thread(target=_writer, daemon=True)
    wt.start()

    print(f"文件: {args.file}  {total} 帧  {w}x{h}  {fps:g}fps  "
          f"格式 0x{fmt_byte:02X} {'整帧' if args.full else 'delta'}",
          file=sys.stderr)

    prev = None
    idx = 0
    sent = 0
    t0 = time.time()
    try:
        while True:
            if idx >= total:
                if args.loop:
                    idx = 0
                    prev = None  # 循环重开需整帧刷新
                else:
                    break
            frame = np.frombuffer(data[idx * frame_bytes:(idx + 1) * frame_bytes],
                                  dtype=np.uint8)
            if args.full or prev is None:
                mask = (1 << n_strips) - 1
            else:
                diff = (frame != prev).reshape(n_strips, -1).any(axis=1)
                mask = 0
                for i, ch in enumerate(diff):
                    if ch:
                        mask |= (1 << i)

            start = time.time()
            if mask == 0:
                wq.put(magic + struct.pack("<H", 0))
            else:
                blocks = frame.reshape(n_strips, strip_bytes)
                changed = [i for i in range(n_strips) if mask & (1 << i)]
                out = bytearray(4 + len(changed) * strip_bytes)
                out[0] = MAGIC0
                out[1] = fmt_byte
                out[2] = (mask & 0xFF)
                out[3] = (mask >> 8)
                pos = 4
                for i in changed:
                    out[pos:pos + strip_bytes] = blocks[i].tobytes()
                    pos += strip_bytes
                wq.put(out)
            prev = frame
            idx += 1
            sent += 1
            if sent % 20 == 0:
                print(f"\r... {sent} 帧, {sent/max(1e-6,time.time()-t0):.2f} fps",
                      end="", file=sys.stderr, flush=True)
            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        wq.put(None)
        wt.join()
        ser.close()
    print(f"\n共发送 {sent} 帧，用时 {time.time()-t0:.1f}s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""把 video_transcode.py 生成的裸 RGB332 帧文件推流到 STM32G474 + ST7789。

播放时不解码、不转色，直接按帧读文件并发送，帧率最稳、CPU 占用最低。

依赖：pyserial

用法：
    python3 host/usb_raw_play.py out.raw
    python3 host/usb_raw_play.py out.raw --fps 8 --loop --port /dev/cu.usbmodemXXXX
"""
import argparse
import struct
import sys
import time

MAGIC0 = 0xA5


def main() -> int:
    ap = argparse.ArgumentParser(description="Play raw RGB332 frames over USB CDC")
    ap.add_argument("file", help="video_transcode.py 生成的 .raw 文件")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341")
    ap.add_argument("--fps", type=float, default=0.0,
                    help="播放帧率；0 表示用文件里的帧率")
    ap.add_argument("--loop", action="store_true")
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
    except ImportError:
        print("缺少 pyserial", file=sys.stderr)
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

    ser = serial.Serial(args.port, 115200, timeout=1)
    ser.dtr = True
    ser.rts = True

    print(f"文件: {args.file}  {total} 帧  {w}x{h}  {fps:g}fps  "
          f"格式 0x{fmt_byte:02X}", file=sys.stderr)
    frames = 0
    sent = 0
    t0 = time.time()
    try:
        while True:
            off = frames * frame_bytes
            if off >= len(data):
                if args.loop:
                    frames = 0
                    continue
                break
            start = time.time()
            ser.write(magic)
            ser.write(data[off:off + frame_bytes])
            ser.flush()
            sent += 1
            frames += 1
            if sent % 20 == 0:
                print(f"\r... {sent} 帧, {sent/max(1e-6,time.time()-t0):.2f} fps",
                      end="", file=sys.stderr, flush=True)
            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
    print(f"\n共发送 {sent} 帧，用时 {time.time()-t0:.1f}s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

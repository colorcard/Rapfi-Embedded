#!/usr/bin/env python3
"""把视频文件通过 Type-C USB CDC 推流到 STM32G474 + ST7789 (280x240)。

帧格式（固件侧解析，次字节 0x5A..0x5D）：
    A5 5A  RGB565 280x240
    A5 5B  RGB332 280x240
    A5 5C  RGB332 140x120（MCU 2x 放大）
    A5 5D  RGB565 140x120（MCU 2x 放大）

依赖：ffmpeg、Pillow、numpy、pyserial。

用法：
    python3 host/usb_video_stream.py video.mp4 --rgb332 --loop
    python3 host/usb_video_stream.py video.mp4 --scale 2 --rgb332
"""
import argparse
import os
import queue
import subprocess
import sys
import threading
import time

W, H = 280, 240


def build_filter(fit: str, tw: int, th: int) -> str:
    if fit == "crop":
        return (f"scale={tw}:{th}:force_original_aspect_ratio=increase,"
                f"crop={tw}:{th}")
    if fit == "stretch":
        return f"scale={tw}:{th}"
    return (f"scale={tw}:{th}:force_original_aspect_ratio=decrease,"
            f"pad={tw}:{th}:(ow-iw)/2:(oh-ih)/2")


def decode_worker(proc, tw: int, th: int, nb: int, rgb332: bool, magic: bytes,
                  out: "queue.Queue", stop: threading.Event):
    import numpy as np  # noqa: WPS433

    frame_bytes = tw * th * 3
    while not stop.is_set():
        buf = proc.stdout.read(frame_bytes)
        if len(buf) < frame_bytes:
            break
        arr = np.frombuffer(buf, dtype=np.uint8).reshape(th, tw, 3)
        if rgb332:
            data = ((arr[:, :, 0] & 0xE0)
                    | ((arr[:, :, 1] & 0xE0) >> 3)
                    | (arr[:, :, 2] >> 6)).astype(np.uint8).tobytes()
        else:
            r5 = (arr[:, :, 0] >> 3).astype(np.uint16)
            g6 = (arr[:, :, 1] >> 2).astype(np.uint16)
            b5 = (arr[:, :, 2] >> 3).astype(np.uint16)
            data = ((r5 << 11) | (g6 << 5) | b5).astype(">u2").tobytes()
        try:
            out.put((magic, data), timeout=0.5)
        except queue.Full:
            pass


def main() -> int:
    ap = argparse.ArgumentParser(description="Video -> ST7789 via USB CDC")
    ap.add_argument("video", help="输入视频文件")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341")
    ap.add_argument("--fps", type=float, default=30.0,
                    help="发送节流上限（串口实际吞吐约 500KB/s）")
    ap.add_argument("--fit", choices=["pad", "crop", "stretch"], default="crop")
    ap.add_argument("--loop", action="store_true", help="循环播放")
    ap.add_argument("--rgb332", action="store_true", help="用 RGB332（数据减半）")
    ap.add_argument("--scale", type=int, choices=[1, 2], default=1)
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
    except ImportError:
        print("缺少 pyserial", file=sys.stderr)
        return 1

    tw, th = W // args.scale, H // args.scale
    fmt_index = (0 if args.scale == 1 else 2) + (1 if args.rgb332 else 0)
    magic = bytes([0xA5, 0x5A + fmt_index])

    cmd = ["ffmpeg", "-v", "error", "-re"]
    if args.loop:
        cmd += ["-stream_loop", "-1"]
    cmd += ["-i", args.video, "-vf", build_filter(args.fit, tw, th),
            "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    ser = serial.Serial(args.port, 115200, timeout=1)
    ser.dtr = True
    ser.rts = True

    q: "queue.Queue" = queue.Queue(maxsize=2)
    stop = threading.Event()
    th_dec = threading.Thread(target=decode_worker,
                              args=(proc, tw, th, 3, args.rgb332, magic, q,
                                    stop),
                              daemon=True)
    th_dec.start()

    period = 1.0 / args.fps if args.fps > 0 else 0.0
    frames = 0
    t0 = time.time()
    try:
        while True:
            magic_b, data = q.get()
            if not magic_b:
                break
            start = time.time()
            ser.write(magic_b)
            ser.write(data)
            ser.flush()
            frames += 1
            if frames % 20 == 0:
                fps = frames / max(1e-6, time.time() - t0)
                print(f"\r... {frames} 帧, {fps:.2f} fps", end="",
                      file=sys.stderr, flush=True)
            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
            if proc.poll() is not None and q.empty():
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        proc.terminate()
        ser.close()
    print(f"\n共推流 {frames} 帧，用时 {time.time()-t0:.1f}s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""通过 Type-C USB CDC 把视频推流到 STM32G474 + ST7789 (280x240)。

固件侧：本工程（去 LVGL 的纯推流固件）会把收到的帧按 16 行条带写入屏幕。
帧格式：魔数 b'A5 5A' + 280*240 个 RGB565 像素（高字节先发 / 大端）。

依赖：ffmpeg（命令行）、pyserial（pip install pyserial）。

用法：
    python3 host/usb_video_stream.py video.mp4
    python3 host/usb_video_stream.py video.mp4 --fps 8 --fit crop
    python3 host/usb_video_stream.py video.mp4 --loop
    # 指定端口（默认见下），macOS 上形如 /dev/cu.usbmodem<UID>1
    python3 host/usb_video_stream.py video.mp4 --port /dev/cu.usbmodem2088388236341
"""
import argparse
import array
import subprocess
import sys
import time

W, H = 280, 240
MAGIC = b"\xA5\x5A"
FRAME_BYTES = W * H * 2


def build_filter(fit: str) -> str:
    if fit == "crop":
        return (f"scale={W}:{H}:force_original_aspect_ratio=increase,"
                f"crop={W}:{H}")
    if fit == "stretch":
        return f"scale={W}:{H}"
    # pad：保持比例，居中留黑边
    return (f"scale={W}:{H}:force_original_aspect_ratio=decrease,"
            f"pad={W}:{H}:(ow-iw)/2:(oh-ih)/2")


def main() -> int:
    ap = argparse.ArgumentParser(description="USB CDC video stream to ST7789")
    ap.add_argument("video", help="输入视频文件")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341",
                    help="USB CDC 串口设备")
    ap.add_argument("--fps", type=float, default=10.0, help="推流帧率")
    ap.add_argument("--fit", choices=["pad", "crop", "stretch"], default="pad",
                    help="画面适配方式")
    ap.add_argument("--loop", action="store_true", help="循环播放")
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
    except ImportError:
        print("缺少 pyserial，请先 pip install pyserial", file=sys.stderr)
        return 1

    cmd = ["ffmpeg", "-v", "error"]
    if args.loop:
        cmd += ["-stream_loop", "-1"]
    cmd += ["-i", args.video, "-vf", build_filter(args.fit),
            "-pix_fmt", "rgb565le", "-f", "rawvideo", "-"]

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    ser = serial.Serial(args.port, 115200, timeout=1)
    ser.dtr = True
    ser.rts = True

    period = 1.0 / args.fps if args.fps > 0 else 0.0
    frames = 0
    t0 = time.time()
    try:
        while True:
            frame = proc.stdout.read(FRAME_BYTES)
            if len(frame) < FRAME_BYTES:
                break  # 视频结束

            # rgb565le -> 大端（ST7789 要求高字节先发）
            arr = array.array("H", frame)
            arr.byteswap()

            start = time.time()
            ser.write(MAGIC)
            ser.write(arr.tobytes())
            ser.flush()
            frames += 1

            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()
        ser.close()

    elapsed = time.time() - t0
    if elapsed > 0:
        print(f"已推流 {frames} 帧，用时 {elapsed:.1f}s，"
              f"平均 {frames / elapsed:.2f} fps")
    return 0


if __name__ == "__main__":
    sys.exit(main())

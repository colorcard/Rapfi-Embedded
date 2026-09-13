#!/usr/bin/env python3
"""实时把电脑画面推到 STM32G474 + ST7789 (280x240)。

macOS 上 ffmpeg 的 avfoundation 屏幕采集会“只给第一帧”，因此改用系统
`screencapture` 逐帧抓屏；并用“抓屏线程 + 发送线程”流水线，让采集与串口传输
重叠，进一步提高帧率。

帧格式（固件侧解析，次字节 0x5A..0x5D）：
    A5 5A  RGB565 280x240
    A5 5B  RGB332 280x240
    A5 5C  RGB332 140x120（MCU 2x 放大）
    A5 5D  RGB565 140x120（MCU 2x 放大）

依赖：Pillow、numpy、pyserial
权限：需在 系统设置→隐私与安全性→屏幕录制 中授权你的终端 App。

用法：
    python3 host/usb_screen_stream.py --rgb332 --fps 15
    python3 host/usb_screen_stream.py --scale 2 --rgb332 --fps 15
"""
import argparse
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time

W, H = 280, 240


def fit_image(im, mode: str, tw: int, th: int):
    from PIL import Image  # noqa: WPS433

    iw, ih = im.size
    if mode == "stretch":
        return im.resize((tw, th)).convert("RGB")
    scale = max(tw / iw, th / ih) if mode == "crop" else min(tw / iw, th / ih)
    resized = im.resize((max(1, round(iw * scale)), max(1, round(ih * scale))),
                        Image.BILINEAR).convert("RGB")
    if mode == "crop":
        left = (resized.width - tw) // 2
        top = (resized.height - th) // 2
        return resized.crop((left, top, left + tw, top + th))
    canvas = Image.new("RGB", (tw, th), (0, 0, 0))
    canvas.paste(resized, ((tw - resized.width) // 2,
                           (th - resized.height) // 2))
    return canvas


def capture_worker(tmp: str, fit: str, tw: int, th: int, rgb332: bool,
                   magic: bytes, out: "queue.Queue", stop: threading.Event):
    import numpy as np  # noqa: WPS433
    from PIL import Image  # noqa: WPS433

    while not stop.is_set():
        subprocess.run(["/usr/sbin/screencapture", "-x", "-t", "tiff", tmp],
                       check=False)
        try:
            with Image.open(tmp) as im:
                arr = np.asarray(fit_image(im, fit, tw, th))
        except Exception:
            continue
        if rgb332:
            px = ((arr[:, :, 0] & 0xE0)
                  | ((arr[:, :, 1] & 0xE0) >> 3)
                  | (arr[:, :, 2] >> 6)).astype(np.uint8)
            data = px.tobytes()
        else:
            r5 = (arr[:, :, 0] >> 3).astype(np.uint16)
            g6 = (arr[:, :, 1] >> 2).astype(np.uint16)
            b5 = (arr[:, :, 2] >> 3).astype(np.uint16)
            data = ((r5 << 11) | (g6 << 5) | b5).astype(">u2").tobytes()
        try:
            out.put((magic, data), timeout=0.5)
        except queue.Full:
            pass  # 队列已满则丢帧，保持低延迟


def main() -> int:
    ap = argparse.ArgumentParser(description="Screen -> ST7789 via USB CDC")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341")
    ap.add_argument("--fps", type=float, default=15.0,
                    help="发送节流上限（串口实际吞吐约 500KB/s）")
    ap.add_argument("--fit", choices=["pad", "crop", "stretch"], default="crop")
    ap.add_argument("--rgb332", action="store_true",
                    help="RGB332（1 字节/像素，数据减半，帧率更高）")
    ap.add_argument("--scale", type=int, choices=[1, 2], default=1, help="分辨率缩放")
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
    except ImportError:
        print("缺少 pyserial，请先 pip install pyserial", file=sys.stderr)
        return 1

    tw, th = W // args.scale, H // args.scale
    fmt_index = (0 if args.scale == 1 else 2) + (1 if args.rgb332 else 0)
    magic = bytes([0xA5, 0x5A + fmt_index])

    ser = serial.Serial(args.port, 115200, timeout=1)
    ser.dtr = True
    ser.rts = True

    tmp = os.path.join(tempfile.gettempdir(), f"_usb_scr_{os.getpid()}.tiff")
    q: "queue.Queue" = queue.Queue(maxsize=2)
    stop = threading.Event()
    th_cap = threading.Thread(target=capture_worker,
                              args=(tmp, args.fit, tw, th, args.rgb332, magic,
                                    q, stop),
                              daemon=True)
    th_cap.start()

    period = 1.0 / args.fps if args.fps > 0 else 0.0
    frames = 0
    t0 = time.time()
    print("开始推流，Ctrl-C 退出 ...", file=sys.stderr)
    try:
        while True:
            magic_b, data = q.get()
            start = time.time()
            ser.write(magic_b)
            ser.write(data)
            ser.flush()
            frames += 1
            if frames % 10 == 0:
                fps = frames / max(1e-6, time.time() - t0)
                print(f"\r... {frames} 帧, {fps:.2f} fps", end="",
                      file=sys.stderr, flush=True)
            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ser.close()
        try:
            os.remove(tmp)
        except OSError:
            pass
    print(f"\n共推流 {frames} 帧", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

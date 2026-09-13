#!/usr/bin/env python3
"""实时把电脑画面推到 STM32G474 + ST7789 (280x240) —— screencapture 逐帧抓屏版。

背景：macOS 上 ffmpeg 的 avfoundation 屏幕采集存在“只给第一帧/不刷新”的问题，
因此改用系统自带 `screencapture` 每次抓一张，再用 Pillow+numpy 缩放并转色推送。

帧格式：
    魔数 b'A5 5A' + RGB565（大端，2 字节/像素）  —— 默认
    魔数 b'A5 5B' + RGB332（1 字节/像素，数据减半，帧率更高）

依赖：Pillow、numpy、pyserial
权限：需在 系统设置→隐私与安全性→屏幕录制 中授权你的终端 App。

用法：
    python3 host/usb_screen_stream.py --fps 3
    python3 host/usb_screen_stream.py --rgb332 --fps 5 --fit crop
"""
import argparse
import os
import subprocess
import sys
import tempfile
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


def main() -> int:
    ap = argparse.ArgumentParser(description="Screen -> ST7789 via USB CDC")
    ap.add_argument("--port", default="/dev/cu.usbmodem2088388236341")
    ap.add_argument("--fps", type=float, default=3.0)
    ap.add_argument("--fit", choices=["pad", "crop", "stretch"], default="crop")
    ap.add_argument("--rgb332", action="store_true",
                    help="用 RGB332（1 字节/像素，数据减半，帧率更高）")
    ap.add_argument("--scale", type=int, choices=[1, 2], default=1,
                    help="源分辨率相对 280x240 的缩小倍数（2=半分辨率，MCU 放大 2x）")
    args = ap.parse_args()

    try:
        import serial  # noqa: WPS433
        from PIL import Image  # noqa: WPS433
        import numpy as np  # noqa: WPS433
    except ImportError as exc:
        print(f"缺少依赖：{exc}（需要 pyserial / pillow / numpy）", file=sys.stderr)
        return 1

    tw, th = W // args.scale, H // args.scale
    # 魔数次字节：0x5A=RGB565全, 0x5B=RGB332全, 0x5C=RGB332半, 0x5D=RGB565半
    fmt_index = (0 if args.scale == 1 else 2) + (1 if args.rgb332 else 0)
    magic = bytes([0xA5, 0x5A + fmt_index])

    ser = serial.Serial(args.port, 115200, timeout=1)
    ser.dtr = True
    ser.rts = True

    tmp = os.path.join(tempfile.gettempdir(), "_usb_scr.tiff")
    period = 1.0 / args.fps if args.fps > 0 else 0.0
    frames = 0
    t0 = time.time()
    print("开始推流，Ctrl-C 退出 ...", file=sys.stderr)
    try:
        while True:
            start = time.time()
            subprocess.run(["/usr/sbin/screencapture", "-x", "-t", "tiff", tmp],
                           check=False)
            try:
                with Image.open(tmp) as im:
                    arr = np.asarray(fit_image(im, args.fit, tw, th))
            except Exception:
                continue
            if args.rgb332:
                px = ((arr[:, :, 0] & 0xE0)
                      | ((arr[:, :, 1] & 0xE0) >> 3)
                      | (arr[:, :, 2] >> 6)).astype(np.uint8)
                payload = px.tobytes()
            else:
                r5 = (arr[:, :, 0] >> 3).astype(np.uint16)
                g6 = (arr[:, :, 1] >> 2).astype(np.uint16)
                b5 = (arr[:, :, 2] >> 3).astype(np.uint16)
                px = ((r5 << 11) | (g6 << 5) | b5)
                payload = px.astype(">u2").tobytes()
            ser.write(magic)
            ser.write(payload)
            ser.flush()
            frames += 1
            if frames % 5 == 0:
                fps = frames / max(1e-6, time.time() - t0)
                print(f"\r... {frames} 帧, {fps:.2f} fps", end="",
                      file=sys.stderr, flush=True)
            dt = time.time() - start
            if period > dt:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        try:
            os.remove(tmp)
        except OSError:
            pass
    print(f"\n共推流 {frames} 帧", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

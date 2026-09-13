#!/usr/bin/env python3
"""把视频预转码为“裸 RGB332 帧序列”文件，供 usb_raw_play.py 直接推流。

输出文件格式（小端）：
    offset 0   : b'R332'      魔数
    offset 4   : u8  fmt      LCD 帧头次字节（0x5B=RGB332 280x240, 0x5C=140x120）
    offset 5   : u16 src_w    源宽
    offset 7   : u16 src_h    源高
    offset 9   : u16 fps      帧率
    offset 11  : 5 bytes 预留
    之后       : 逐帧 RGB332（src_w*src_h 字节/帧）

依赖：ffmpeg、numpy。

用法：
    python3 host/video_transcode.py in.mp4 out.raw --fps 8
    python3 host/video_transcode.py in.mp4 out_half.raw --fps 8 --scale 2
"""
import argparse
import os
import struct
import subprocess
import sys

W, H = 280, 240


def build_filter(fit: str, tw: int, th: int) -> str:
    if fit == "crop":
        return (f"scale={tw}:{th}:force_original_aspect_ratio=increase,"
                f"crop={tw}:{th}")
    if fit == "stretch":
        return f"scale={tw}:{th}"
    return (f"scale={tw}:{th}:force_original_aspect_ratio=decrease,"
            f"pad={tw}:{th}:(ow-iw)/2:(oh-ih)/2")


def main() -> int:
    ap = argparse.ArgumentParser(description="Video -> raw RGB332 frames")
    ap.add_argument("video")
    ap.add_argument("output")
    ap.add_argument("--fps", type=float, default=8.0, help="转码帧率")
    ap.add_argument("--scale", type=int, choices=[1, 2], default=1)
    ap.add_argument("--fit", choices=["pad", "crop", "stretch"], default="crop")
    args = ap.parse_args()

    try:
        import numpy as np  # noqa: WPS433
    except ImportError:
        print("缺少 numpy", file=sys.stderr)
        return 1

    tw, th = W // args.scale, H // args.scale
    fmt_byte = 0x5C if args.scale == 2 else 0x5B

    cmd = ["ffmpeg", "-v", "error", "-i", args.video,
           "-vf", build_filter(args.fit, tw, th), "-r", str(args.fps),
           "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    frame_bytes = tw * th * 3
    count = 0
    with open(args.output, "wb") as fp:
        fp.write(b"R332")
        fp.write(struct.pack("<BHHH", fmt_byte, tw, th, int(args.fps)))
        fp.write(b"\x00" * 5)
        while True:
            buf = proc.stdout.read(frame_bytes)
            if len(buf) < frame_bytes:
                break
            arr = np.frombuffer(buf, dtype=np.uint8).reshape(th, tw, 3)
            px = ((arr[:, :, 0] & 0xE0)
                  | ((arr[:, :, 1] & 0xE0) >> 3)
                  | (arr[:, :, 2] >> 6)).astype(np.uint8)
            fp.write(px.tobytes())
            count += 1
            if count % 50 == 0:
                print(f"\r... {count} 帧", end="", file=sys.stderr, flush=True)
    proc.wait()
    size = os.path.getsize(args.output)
    print(f"\n完成：{args.output}  {count} 帧  {size/1024/1024:.1f} MB  "
          f"({tw}x{th} RGB332 @ {args.fps:g}fps)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

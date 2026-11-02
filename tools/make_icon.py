# 生成 StickySnap 应用图标（红色图钉，32x32 RGBA，PNG-in-ICO）
# 纯标准库实现，避免依赖 Pillow：python tools/make_icon.py res/app.ico
import math
import os
import struct
import sys
import zlib


def make_png(width, height, get_pixel):
    raw = b""
    for y in range(height):
        raw += b"\x00"  # filter: none
        for x in range(width):
            raw += bytes(get_pixel(x, y))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def pixel(x, y):
    cx, cy = 16.0, 13.0
    d = math.hypot(x - cx, y - cy)

    # 斜针（先画，圆头会盖住上半段）
    px, py, qx, qy = 13.0, 21.0, 7.0, 30.0
    vx, vy = qx - px, qy - py
    length = vx * vx + vy * vy
    t = 0.0 if length == 0 else ((x - px) * vx + (y - py) * vy) / length
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    bx, by = px + t * vx, py + t * vy
    if math.hypot(x - bx, y - by) <= 1.6:
        return (205, 205, 210, 255)

    # 圆头
    if d <= 11.0:
        r, g, b = 226, 62, 62
        if d > 8.5:
            r, g, b = 180, 40, 40  # 边缘压暗
        if d <= 4.5:
            r, g, b = 255, 255, 255  # 白色高光
        a = 255 if d <= 10.2 else int(255 * (11.0 - d) / 0.8)
        if a > 0:
            return (r, g, b, a)
    return (0, 0, 0, 0)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "app.ico"
    png = make_png(32, 32, pixel)
    entry = struct.pack("<BBBBHHII", 32, 32, 0, 0, 1, 32, len(png), 22)
    ico = struct.pack("<HHH", 0, 1, 1) + entry + png
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "wb") as f:
        f.write(ico)
    print(f"wrote {out} ({len(ico)} bytes)")


if __name__ == "__main__":
    main()

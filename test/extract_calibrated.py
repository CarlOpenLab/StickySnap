# 带标定的图标条提取：
#   test/ref/lucide_full.png（浏览器整屏截图，含 3 个已知 css 位置的黑色标定块）
#   → 反推 css→image 的线性映射（缩放+偏移）→ 精确采样 11 个 Lucide 图标
#   → res/toolbar.png（352x32 白色 + alpha）
# 纯标准库。
import struct
import zlib

PAGE_W = 1280          # 页面 css 宽度（icon_render.html 的 window.innerWidth）
MARKERS = [            # css 坐标（块中心），块 4x4
    (2, 12),
    (PAGE_W - 2, 12),
    (2, 92),
]
CELL = 64
GAP = 0
TOP_Y = 0
N = 11

def decode_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    width = height = ctype = None
    idat = b""
    while pos < len(data):
        ln, tag = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            width, height, _, ctype = struct.unpack(">IIBB", chunk[:10])
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
        pos += 12 + ln
    ch = 3 if ctype == 2 else 4
    raw = zlib.decompress(idat)
    stride = width * ch
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        ft = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if ft == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, ch, out

def lum_at(img, x, y):
    """双线性采样亮度"""
    w, h, ch, pix = img
    x = max(0.0, min(w - 1.0, x))
    y = max(0.0, min(h - 1.0, y))
    x0, y0 = int(x), int(y)
    fx, fy = x - x0, y - y0
    x1 = min(x0 + 1, w - 1)
    y1 = min(y0 + 1, h - 1)

    def l(xx, yy):
        o = (yy * w + xx) * ch
        b, g, r = pix[o], pix[o + 1], pix[o + 2]
        return (r * 299 + g * 587 + b * 114) // 1000

    return (l(x0, y0) * (1 - fx) * (1 - fy) + l(x1, y0) * fx * (1 - fy)
            + l(x0, y1) * (1 - fx) * fy + l(x1, y1) * fx * fy)

def find_marker(img, cx, cy, r=26):
    """在预测位置附近找暗块质心"""
    w, h, ch, pix = img
    sx_total = sy_total = cnt = 0
    for yy in range(max(0, int(cy - r)), min(h, int(cy + r) + 1)):
        for xx in range(max(0, int(cx - r)), min(w, int(cx + r) + 1)):
            o = (yy * w + xx) * ch
            if pix[o] < 230 and pix[o + 1] < 230 and pix[o + 2] < 230:
                sx_total += xx
                sy_total += yy
                cnt += 1
    return (sx_total / cnt, sy_total / cnt, cnt) if cnt else None

def png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

def main():
    img = decode_png(r"test/ref/lucide_full.png")
    w_img, h_img, _, _ = img
    print(f"image: {w_img}x{h_img}")

    s_guess = w_img / PAGE_W
    measured = []
    for (mx, my) in MARKERS:
        pred = (mx * s_guess, my * s_guess)
        m = find_marker(img, pred[0], pred[1])
        measured.append(m)
        print(f"marker css({mx},{my}) predicted({pred[0]:.1f},{pred[1]:.1f}) -> {m}")

    assert all(m and m[2] >= 2 for m in measured), "marker not found"

    # x 标定：marker0(中心 css 2) 与 marker1(中心 css PAGE_W-2)
    ax = measured[0][0]
    bx = measured[1][0]
    sx = (bx - ax) / (PAGE_W - 2 - 2)
    ox = ax - 2 * sx
    # y 标定：marker0(中心 css 12) 与 marker2(中心 css 92)
    ay = measured[0][1]
    cy = measured[2][1]
    sy = (cy - ay) / (92 - 12)
    oy = ay - 12 * sy
    print(f"calibration: sx={sx:.4f} ox={ox:.2f} sy={sy:.4f} oy={oy:.2f}")

    def css_to_img(cx, cy):
        return (ox + cx * sx, oy + cy * sy)

    rows = []
    for ty in range(32):
        row = bytearray()
        for tx in range(32):
            for i in range(N):
                pass
        row = None
        break
    # 逐图标生成 32x32
    strips = []
    for i in range(N):
        cell_rows = []
        for ty in range(32):
            row = bytearray()
            for tx in range(32):
                css_x = (i * (CELL + GAP)) + 18 + (tx + 0.5) * 28 / 32.0
                css_y = TOP_Y + 18 + (ty + 0.5) * 28 / 32.0
                ix, iy = css_to_img(css_x, css_y)
                # 4 点子采样取平均（覆盖输出像素约 ±CELL/64 css 范围）
                vals = []
                for ddx, ddy in ((-0.4, -0.4), (0.4, -0.4), (-0.4, 0.4), (0.4, 0.4)):
                    jx, jy = css_to_img(css_x + ddx * CELL / 32.0,
                                        css_y + ddy * CELL / 32.0)
                    vals.append(lum_at(img, jx, jy))
                avg = sum(vals) / 4.0
                alpha = max(0, min(255, round(255 - avg)))
                row += bytes((255, 255, 255, alpha))
            cell_rows.append(bytes(row))
        strips.append(cell_rows)

    def row_fn(x, y):
        c = strips[x // 32]
        return c[y][(x % 32) * 4:(x % 32) * 4 + 4]

    raw = b""
    for y in range(32):
        raw += b"\x00"
        for x in range(32 * N):
            raw += bytes(row_fn(x, y))
    ihdr = struct.pack(">IIBBBBB", 32 * N, 32, 8, 6, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
           + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))
    open(r"res/toolbar.png", "wb").write(png)
    print(f"wrote res/toolbar.png ({32 * N}x32, {len(png)} bytes)")

if __name__ == "__main__":
    main()

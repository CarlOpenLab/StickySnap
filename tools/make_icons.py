# StickySnap 图标生成器
# 图标设计来自 Lucide (https://lucide.dev, ISC License) —— 官方 path 数据，
# 按 Lucide 规范渲染：24x24 网格 / 1.5px 描边 / 圆头圆角，8 倍超采样抗锯齿，
# 24px 图标居中嵌入 32px 单元（四周 4px 留白）。
# 输出：
#   res/toolbar.png        —— 11 个灰色图标（常态，#C9CED6 不透明）
#   res/toolbar_active.png —— 11 个白色图标（悬停/激活态）
#   res/app.ico            —— App/托盘图钉图标（16/32/48）
# 纯标准库。用法：python tools/make_icons.py
import math
import os
import re
import struct
import zlib

SS = 8          # 每像素超采样
CELL = 32       # 输出单元
ICON = 16       # 图标绘制区（24 网格缩放到 16px，落地页 xbtn 比例）
PAD = (CELL - ICON) // 2   # 8
STROKE = 1.85   # 描边宽（24 网格单位；16px 渲染下约 1.2px，视觉对齐落地页）

COLOR_NORMAL = (199, 205, 216)  # 落地页 .xbtn color #C7CDD8
COLOR_ACTIVE = (244, 245, 247)  # 落地页 --fg #F4F5F7

# ---------------- Lucide 官方 path 数据（ISC License, (c) Lucide Contributors） ----------------
ICONS = {
    "rect":    [("rect", 4, 6.5, 16, 11, 1)],
    "ellipse": [("ellipse", 12, 12, 8, 5.5)],
    "arrow":   ["M6 18 L18 6", "M12.5 6 H18 V11.5"],
    "pen":     ["M13.5 6.5 l4 4",
                "M4 20 l1.2-4.6 10.8-10.8 a2 2 0 0 1 2.8 0 l1.6 1.6 a2 2 0 0 1 0 2.8 L9.6 19.8 4 20 Z"],
    "marker":  ["M4 20h7", "M8 16l7.5-11 3.4 2.3L11.4 18l-4.6.9L8 16Z"],
    "mosaic":  ["M4 4h7v7H4z", "M13 4h7v7h-7z", "M4 13h7v7H4z", "M13 13h7v7h-7z"],
    "undo":    ["M7.5 5 3.5 8.5 7.5 12", "M4 8.5 H15 a5.5 5.5 0 0 1 0 11 H8"],
    "pin":     ["M9.5 3.5h5l-.7 4.2 2.7 3.3H7.5l2.7-3.3L9.5 3.5Z", "M12 11v9"],
    "copy":    [("rect", 9, 9, 11, 11, 2), "M5 15V5a2 2 0 0 1 2-2h10"],
    "save":    ["M5 4h11l3 3v13H5z", "M8 4v5h7V4", "M8 20v-6h8v6"],
    "close":   ["M6 6l12 12M18 6L6 18"],
}
CELL_ORDER = ["rect", "ellipse", "arrow", "pen", "marker", "mosaic",
              "undo", "pin", "copy", "save", "close"]

# ---------------- SVG path 解析与展平 ----------------
NUM = r"-?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?"
TOKEN = re.compile(r"([MmLlHhVvCcSsQqTtAaZz])|(" + NUM + ")")

def flatten_cubic(cur, x0, y0, x1, y1, x2, y2, x3, y3, steps=14):
    for k in range(1, steps + 1):
        t = k / steps
        mt = 1 - t
        cur.append((mt**3 * x0 + 3 * mt * mt * t * x1 + 3 * mt * t * t * x2 + t**3 * x3,
                    mt**3 * y0 + 3 * mt * mt * t * y1 + 3 * mt * t * t * y2 + t**3 * y3))

def flatten_quad(cur, x0, y0, x1, y1, x2, y2, steps=10):
    for k in range(1, steps + 1):
        t = k / steps
        mt = 1 - t
        cur.append((mt * mt * x0 + 2 * mt * t * x1 + t * t * x2,
                    mt * mt * y0 + 2 * mt * t * y1 + t * t * y2))

def arc_segments(cur, x1, y1, rx, ry, phi_deg, fa, fs, x2, y2, steps=24):
    rx, ry = abs(rx), abs(ry)
    if rx == 0 or ry == 0:
        cur.append((x2, y2))
        return
    phi = math.radians(phi_deg)
    cp, sp = math.cos(phi), math.sin(phi)
    dx2, dy2 = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    x1p = cp * dx2 + sp * dy2
    y1p = -sp * dx2 + cp * dy2
    lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry)
    if lam > 1:
        s = math.sqrt(lam)
        rx *= s
        ry *= s
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    co = math.sqrt(max(0.0, num / den)) if den else 0.0
    if fa == fs:
        co = -co
    cxp = co * rx * y1p / ry
    cyp = -co * ry * x1p / rx
    cx = cp * cxp - sp * cyp + (x1 + x2) / 2.0
    cy = sp * cxp + cp * cyp + (y1 + y2) / 2.0

    def ang(ux, uy, vx, vy):
        l = math.hypot(ux, uy) * math.hypot(vx, vy)
        a = math.acos(max(-1.0, min(1.0, (ux * vx + uy * vy) / l))) if l else 0.0
        return -a if (ux * vy - uy * vx < 0) else a

    th1 = ang(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dth = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not fs and dth > 0:
        dth -= 2 * math.pi
    if fs and dth < 0:
        dth += 2 * math.pi
    for k in range(1, steps + 1):
        t = th1 + dth * k / steps
        cur.append((cx + rx * math.cos(t) * cp - ry * math.sin(t) * sp,
                    cy + rx * math.cos(t) * sp + ry * math.sin(t) * cp))

def parse_path(d):
    """SVG path d → 折线列表（24 网格坐标）"""
    toks = [(mm.group(1), mm.group(2)) for mm in TOKEN.finditer(d)]
    polys, cur = [], []
    cx = cy = sx = sy = 0.0
    i, cmd = 0, None
    prev_c2 = prev_q = None

    def nums():
        nonlocal i
        out = []
        while i < len(toks) and toks[i][0] is None:
            out.append(float(toks[i][1]))
            i += 1
        return out

    while i < len(toks):
        c, _ = toks[i]
        if c is not None:
            cmd = c
            i += 1
        start = i
        n = nums()
        if cmd in ("M", "m"):
            if cur:
                polys.append(cur)
                cur = []
            x, y = n[0], n[1]
            if cmd == "m":
                x += cx
                y += cy
            cx, cy, sx, sy = x, y, x, y
            cur.append((x, y))
            if len(n) > 2:
                i = start + 2  # M/m 后的多余数字 = 隐式 L/l，回退重新处理
            cmd = "L" if cmd == "M" else "l"
        elif cmd in ("L", "l"):
            for k in range(0, len(n) - 1, 2):
                x, y = n[k], n[k + 1]
                if cmd == "l":
                    x += cx
                    y += cy
                cur.append((x, y))
                cx, cy = x, y
        elif cmd in ("H", "h"):
            for x in n:
                if cmd == "h":
                    x += cx
                cur.append((x, cy))
                cx = x
        elif cmd in ("V", "v"):
            for y in n:
                if cmd == "v":
                    y += cy
                cur.append((cx, y))
                cy = y
        elif cmd in ("C", "c"):
            for k in range(0, len(n) - 5, 6):
                x1, y1, x2, y2, x, y = n[k:k + 6]
                if cmd == "c":
                    x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy
                flatten_cubic(cur, cx, cy, x1, y1, x2, y2, x, y)
                prev_c2 = (x2, y2)
                cx, cy = x, y
        elif cmd == "S":
            for k in range(0, len(n) - 3, 4):
                x2, y2, x, y = n[k:k + 4]
                if cmd == "s":
                    x2 += cx; y2 += cy; x += cx; y += cy
                x1, y1 = (2 * cx - prev_c2[0], 2 * cy - prev_c2[1]) if prev_c2 else (cx, cy)
                flatten_cubic(cur, cx, cy, x1, y1, x2, y2, x, y)
                prev_c2 = (x2, y2)
                cx, cy = x, y
        elif cmd in ("Q", "q"):
            for k in range(0, len(n) - 3, 4):
                x1, y1, x, y = n[k:k + 4]
                if cmd == "q":
                    x1 += cx; y1 += cy; x += cx; y += cy
                flatten_quad(cur, cx, cy, x1, y1, x, y)
                prev_q = (x1, y1)
                cx, cy = x, y
        elif cmd == "T":
            for k in range(0, len(n) - 1, 2):
                x, y = n[k], n[k + 1]
                if cmd == "t":
                    x += cx; y += cy
                x1, y1 = (2 * cx - prev_q[0], 2 * cy - prev_q[1]) if prev_q else (cx, cy)
                flatten_quad(cur, cx, cy, x1, y1, x, y)
                prev_q = (x1, y1)
                cx, cy = x, y
        elif cmd in ("A", "a"):
            for k in range(0, len(n) - 6, 7):
                rx, ry, rot, fa, fs, x, y = n[k:k + 7]
                if cmd == "a":
                    x += cx; y += cy
                arc_segments(cur, cx, cy, rx, ry, rot, fa, fs, x, y)
                cx, cy = x, y
        elif cmd in ("Z", "z"):
            if cur:
                cur.append((sx, sy))
                polys.append(cur)
                cur = []
            cx, cy = sx, sy
    if cur:
        polys.append(cur)
    return polys

def lucide_segments(spec):
    """spec 列表 → 线段列表 [(ax,ay,bx,by)]（24 网格坐标）"""
    segs = []
    for item in spec:
        if isinstance(item, tuple):
            if item[0] == "rect":
                _, x, y, w, h, rx = item
                polylines = parse_path(
                    f"M{x+rx} {y}H{x+w-rx}A{rx} {rx} 0 0 1 {x+w} {y+rx}V{y+h-rx}"
                    f"A{rx} {rx} 0 0 1 {x+w-rx} {y+h}H{x+rx}A{rx} {rx} 0 0 1 {x} {y+h-rx}"
                    f"V{y+rx}A{rx} {rx} 0 0 1 {x+rx} {y}Z")
            elif item[0] == "circle":
                _, cx, cy, r = item
                polylines = [parse_path(
                    f"M{cx+r} {cy}A{r} {r} 0 1 1 {cx-r} {cy}A{r} {r} 0 1 1 {cx+r} {cy}Z")[0]]
            else:  # ellipse
                _, cx, cy, rx, ry = item
                polylines = [parse_path(
                    f"M{cx+rx} {cy}A{rx} {ry} 0 1 1 {cx-rx} {cy}A{rx} {ry} 0 1 1 {cx+rx} {cy}Z")[0]]
            for poly in polylines:
                for k in range(len(poly) - 1):
                    segs.append((poly[k][0], poly[k][1], poly[k + 1][0], poly[k + 1][1]))
        else:
            for poly in parse_path(item):
                for k in range(len(poly) - 1):
                    segs.append((poly[k][0], poly[k][1], poly[k + 1][0], poly[k + 1][1]))
    return segs

# ---------------- 渲染（24 网格 1:1 → 32 单元居中） ----------------

def build_icon(segs, color):
    hw = STROKE / 2.0
    boxes = [(min(ax, bx) - hw - 0.5, min(ay, by) - hw - 0.5,
              max(ax, bx) + hw + 0.5, max(ay, by) + hw + 0.5)
             for (ax, ay, bx, by) in segs]

    k = 24.0 / ICON
    icon_rows = []
    for Y in range(ICON):
        row = bytearray()
        for X in range(ICON):
            hit = 0
            for sy in range(SS):
                yy = (Y + (sy + 0.5) / SS) * k
                for sx in range(SS):
                    xx = (X + (sx + 0.5) / SS) * k
                    for si, (ax, ay, bx, by) in enumerate(segs):
                        b = boxes[si]
                        if xx < b[0] or xx > b[2] or yy < b[1] or yy > b[3]:
                            continue
                        pax, pay = xx - ax, yy - ay
                        bax, bay = bx - ax, by - ay
                        dd = bax * bax + bay * bay
                        h = 0.0 if dd == 0 else max(0.0, min(1.0, (pax * bax + pay * bay) / dd))
                        if math.hypot(pax - h * bax, pay - h * bay) <= hw:
                            hit += 1
                            break
            a = round(255 * hit / (SS * SS))
            row += bytes((color[0], color[1], color[2], a))
        icon_rows.append(bytes(row))
    return icon_rows

def embed_cell(icon_rows):
    """24x24 图标居中放进 32x32（其余全透明）"""
    rows = []
    for Y in range(CELL):
        row = bytearray()
        for X in range(CELL):
            ix, iy = X - PAD, Y - PAD
            if 0 <= ix < ICON and 0 <= iy < ICON:
                row += icon_rows[iy][ix * 4:ix * 4 + 4]
            else:
                row += bytes((0, 0, 0, 0))
        rows.append(bytes(row))
    return rows

def png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

def make_png(width, height, row_fn):
    raw = b""
    for y in range(height):
        raw += b"\x00"
        for x in range(width):
            raw += bytes(row_fn(x, y))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
            + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))

def build_strip(out_path, color):
    cells = [embed_cell(build_icon(lucide_segments(ICONS[name]), color))
             for name in CELL_ORDER]
    n = len(cells)

    def px(x, y):
        c = cells[x // CELL]
        return c[y][(x % CELL) * 4:(x % CELL) * 4 + 4]

    png = make_png(CELL * n, CELL, px)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(png)
    print(f"wrote {out_path} ({n} icons, {CELL * n}x{CELL}, {len(png)} bytes)")

# ---------------- App 图钉图标（自绘 SDF，16/32/48） ----------------

def sd_seg(px, py, ax, ay, bx, by):
    pax, pay = px - ax, py - ay
    bax, bay = bx - ax, by - ay
    dd = bax * bax + bay * bay
    h = 0.0 if dd == 0 else max(0.0, min(1.0, (pax * bax + pay * bay) / dd))
    return math.hypot(pax - h * bax, pay - h * bay)

def sd_circle(px, py, cx, cy, r):
    return math.hypot(px - cx, py - cy) - r

def pushpin_rows(size):
    rows = []
    for Y in range(size):
        row = bytearray()
        for X in range(size):
            x = (X + 0.5) * 32.0 / size
            y = (Y + 0.5) * 32.0 / size
            hit_needle = sd_seg(x, y, 13, 21, 7, 30) <= 1.7
            d_head = sd_circle(x, y, 16, 13, 11.0)
            if not (hit_needle or d_head <= 0):
                row += bytes((0, 0, 0, 0))
                continue
            if hit_needle and d_head > 0:
                row += bytes((205, 205, 212, 255))
                continue
            r, g, b = 226, 62, 62
            if d_head > -1.4:
                r, g, b = 178, 40, 40
            if sd_circle(x, y, 12.5, 9.5, 3.6) <= 0:
                r, g, b = 255, 255, 255
            row += bytes((r, g, b, 255))
        rows.append(bytes(row))
    return rows

def build_app_ico(out_path):
    entries = []
    for size in (16, 32, 48):
        rows = pushpin_rows(size)
        entries.append((size, make_png(size, size,
                                       lambda x, y, rows=rows: rows[y][x * 4:x * 4 + 4])))
    ico = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    for size, png in entries:
        ico += struct.pack("<BBBBHHII", size, size, 0, 0, 1, 32, len(png), offset)
        offset += len(png)
    for _, png in entries:
        ico += png
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(ico)
    print(f"wrote {out_path} ({len(ico)} bytes, sizes 16/32/48)")

def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_strip(os.path.join(base, "res", "toolbar.png"), COLOR_NORMAL)
    build_strip(os.path.join(base, "res", "toolbar_active.png"), COLOR_ACTIVE)
    build_app_ico(os.path.join(base, "res", "app.ico"))

if __name__ == "__main__":
    main()

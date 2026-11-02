# 从浏览器截图提取 StickySnap 图标条：
#   test/ref/lucide_2x.png (704x64, 黑图标/白底) -> res/toolbar.png (352x32 白色+alpha)
# 纯标准库 PNG 解码（8bit RGB/RGBA，filter 0-4）+ 2x2 盒式降采样
import struct
import sys
import zlib

def decode_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a png"
    pos = 8
    width = height = bitd = ctype = None
    idat = b""
    while pos < len(data):
        ln, tag = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            width, height, bitd, ctype = struct.unpack(">IIBB", chunk[:10])
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
        pos += 12 + ln
    assert bitd == 8 and ctype in (2, 6), f"unsupported png type {ctype}"
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
        if ft == 1:  # Sub
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 0xFF
        elif ft == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:  # Average
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:  # Paeth
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

def png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

def write_png(path, width, height, row_fn):
    raw = b""
    for y in range(height):
        raw += b"\x00"
        for x in range(width):
            raw += bytes(row_fn(x, y))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
           + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))
    open(path, "wb").write(png)
    return len(png)

def main():
    src = r"test/ref/lucide_2x.png"
    dst = r"res/toolbar.png"
    w, h, ch, pix = decode_png(src)

    def lum(x, y):
        o = (y * w + x) * ch
        b, g, r = pix[o], pix[o + 1], pix[o + 2]
        return (r * 299 + g * 587 + b * 114) // 1000

    # 704x64 -> 11 个 64x64 单元 -> 每个 2x2 降采样到 32x32
    cells = 11
    cell_src = w // cells          # 64
    factor = cell_src // 32        # 2
    assert cell_src == 32 * factor

    def row(x, y):
        # 单元 i，目标像素 (32x32)
        i = x // 32
        tx, ty = x % 32, y
        sx0, sy0 = i * cell_src + tx * factor, ty * factor
        total = 0
        for dy in range(factor):
            for dx in range(factor):
                total += lum(sx0 + dx, sy0 + dy)
        avg = total // (factor * factor)
        alpha = 255 - avg  # 黑=不透明，白=透明
        return (255, 255, 255, alpha)

    size = write_png(dst, 32 * cells, 32, row)
    print(f"wrote {dst} ({32 * cells}x32, {size} bytes) from {w}x{h} source")

if __name__ == "__main__":
    main()

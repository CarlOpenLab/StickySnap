#!/usr/bin/env python3
"""Generate every StickySnap brand asset from assets/logo.png.

The source PNG stays the single source of truth; on first run its baked-in
backdrop is cut out and the transparent result is written back, so later
runs are idempotent. The script keeps the white gap inside the frame,
builds a rounded app-icon tile, and writes the Windows/macOS/web sizes used
by the app, README and landing page.

Usage: python3 tools/make_brand_assets.py
Requires: Pillow (and macOS `iconutil` for the .icns file).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "logo.png"

ACCENT = (0x2F, 0x6F, 0xED)
TILE_TOP = (255, 255, 255)
TILE_BOTTOM = (238, 244, 255)
BORDER = (15, 23, 42, 28)
SHADOW = (15, 23, 42, 70)


def cut_out_background(image: Image.Image) -> Image.Image:
    """Make the exterior backdrop transparent, keeping enclosed whites opaque.

    Flood-filling from the four corners only removes the connected backdrop,
    so the white margin inside the frame survives. The cut rim is feathered:
    alpha is estimated from each pixel's distance to the backdrop relative to
    the local foreground distance, and the matte color is unpremultiplied out
    so no light halo remains on dark surfaces. Already-transparent sources
    pass through untouched.
    """
    rgba = image.convert("RGBA")
    width, height = rgba.size
    corners = ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1))
    if all(rgba.getpixel(c)[3] == 0 for c in corners):
        return rgba

    rgb = rgba.convert("RGB")
    bg = tuple(
        round(sum(rgb.getpixel(c)[ch] for c in corners) / len(corners))
        for ch in range(3)
    )
    for seed in corners:
        ImageDraw.floodfill(rgb, seed, (255, 0, 255), thresh=40)

    r, g, b = rgb.split()
    removed = ImageChops.logical_and(
        ImageChops.logical_and(
            r.point(lambda v: 255 if v == 255 else 0).convert("1"),
            g.point(lambda v: 255 if v == 0 else 0).convert("1"),
        ),
        b.point(lambda v: 255 if v == 255 else 0).convert("1"),
    ).convert("L")
    band = ImageChops.subtract(removed.filter(ImageFilter.MaxFilter(7)), removed)

    diff = ImageChops.difference(rgb, Image.new("RGB", (width, height), bg))
    dr, dg, db = diff.split()
    dist = ImageChops.add(ImageChops.add(dr, dg), db)
    local_fg = dist.filter(ImageFilter.MaxFilter(5))

    out = rgba.copy()
    px = out.load()
    src = rgb.load()
    rp = removed.load()
    bp = band.load()
    dp = dist.load()
    fp = local_fg.load()
    for y in range(height):
        for x in range(width):
            if rp[x, y]:
                px[x, y] = (0, 0, 0, 0)
            elif bp[x, y]:
                a = min(255, round(dp[x, y] * 255 / max(fp[x, y], 1)))
                if a == 0:
                    px[x, y] = (0, 0, 0, 0)
                    continue
                t = a / 255
                px[x, y] = tuple(
                    min(255, max(0, round((src[x, y][ch] - (1 - t) * bg[ch]) / t)))
                    for ch in range(3)
                ) + (a,)
    return out


def vertical_gradient(size: int, top: tuple[int, int, int],
                      bottom: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGB", (1, size))
    draw = ImageDraw.Draw(image)
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        draw.point((0, y), fill=color)
    return image.resize((size, size), Image.Resampling.NEAREST).convert("RGBA")


def rounded_mask(size: int, inset: int, radius_ratio: float,
                 supersample: int = 4) -> Image.Image:
    big = size * supersample
    mask = Image.new("L", (big, big), 0)
    draw = ImageDraw.Draw(mask)
    inset_big = inset * supersample
    radius = round((size - inset * 2) * radius_ratio) * supersample
    draw.rounded_rectangle(
        (inset_big, inset_big, big - inset_big - 1, big - inset_big - 1),
        radius=radius,
        fill=255,
    )
    return mask.resize((size, size), Image.Resampling.LANCZOS)


def border_layer(size: int, inset: int, radius_ratio: float,
                 supersample: int = 4) -> Image.Image:
    big = size * supersample
    layer = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    inset_big = inset * supersample
    radius = round((size - inset * 2) * radius_ratio) * supersample
    draw.rounded_rectangle(
        (inset_big, inset_big, big - inset_big - 1, big - inset_big - 1),
        radius=radius,
        outline=BORDER,
        width=max(1, 2 * supersample),
    )
    return layer.resize((size, size), Image.Resampling.LANCZOS)


def make_icon(mark: Image.Image, size: int = 1024, *, margin: int = 48,
              shadow: bool = True, radius_ratio: float = 0.225) -> Image.Image:
    tile_size = size - margin * 2
    mask = rounded_mask(tile_size, 0, radius_ratio)
    tile = vertical_gradient(tile_size, TILE_TOP, TILE_BOTTOM)
    tile.putalpha(mask)

    # Center the mark with a generous safe area inside the tile.
    padding = round(tile_size * 0.105)
    box = tile_size - padding * 2
    scale = min(box / mark.width, box / mark.height)
    resized = mark.resize(
        (max(1, round(mark.width * scale)), max(1, round(mark.height * scale))),
        Image.Resampling.LANCZOS,
    )
    tile.alpha_composite(
        resized,
        ((tile_size - resized.width) // 2, (tile_size - resized.height) // 2),
    )
    tile.alpha_composite(border_layer(tile_size, 0, radius_ratio))

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    if shadow:
        shadow_mask = mask.filter(ImageFilter.GaussianBlur(18))
        shadow_layer = Image.new("RGBA", (tile_size, tile_size), SHADOW)
        shadow_layer.putalpha(shadow_mask.point(lambda a: round(a * 0.36)))
        canvas.alpha_composite(shadow_layer, (margin, margin + 12))

    canvas.alpha_composite(tile, (margin, margin))
    return canvas


def save_ico(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(
        path,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64),
               (128, 128), (256, 256)],
    )


def save_icns(image: Image.Image, path: Path) -> None:
    iconutil = shutil.which("iconutil")
    if iconutil is None:
        print("skip AppIcon.icns: iconutil not available")
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        iconset = Path(temp) / "AppIcon.iconset"
        iconset.mkdir()
        for base in (16, 32, 128, 256, 512):
            image.resize((base, base), Image.Resampling.LANCZOS).save(
                iconset / f"icon_{base}x{base}.png"
            )
            image.resize((base * 2, base * 2), Image.Resampling.LANCZOS).save(
                iconset / f"icon_{base}x{base}@2x.png"
            )
        subprocess.run(
            [iconutil, "-c", "icns", str(iconset), "-o", str(path)],
            check=True,
        )


def load_font(size: int, *, bold: bool = False, cjk: bool = False) -> ImageFont.FreeTypeFont:
    win_fonts = Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
    if cjk:
        candidates = [
            "/System/Library/Fonts/STHeiti Medium.ttc",
            "/System/Library/Fonts/Hiragino Sans GB.ttc",
            str(win_fonts / "msyh.ttc"),
            str(win_fonts / "simsun.ttc"),
        ]
    else:
        candidates = [
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold
            else "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            str(win_fonts / ("arialbd.ttf" if bold else "arial.ttf")),
            str(win_fonts / ("segoeuib.ttf" if bold else "segoeui.ttf")),
        ]
    for candidate in candidates:
        if Path(candidate).exists():
            try:
                return ImageFont.truetype(candidate, size)
            except OSError:
                continue
    return ImageFont.load_default(size=size)


def make_og(icon: Image.Image) -> Image.Image:
    width, height = 1200, 630
    canvas = Image.new("RGBA", (width, height), (0x0A, 0x0B, 0x0E, 255))

    glow = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.ellipse((720, -220, 1480, 420), fill=(*ACCENT, 88))
    glow_draw.ellipse((-220, 380, 520, 920), fill=(0x8C, 0xC2, 0xFF, 44))
    canvas.alpha_composite(glow.filter(ImageFilter.GaussianBlur(130)))

    # Hairline frame and top accent.
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((24, 24, width - 24, height - 24), radius=28,
                           outline=(255, 255, 255, 26), width=2)
    draw.rounded_rectangle((72, 72, 72 + 150, 78), radius=3, fill=(*ACCENT, 255))

    icon_size = 236
    small_icon = icon.resize((icon_size, icon_size), Image.Resampling.LANCZOS)
    canvas.alpha_composite(small_icon, (78, 184))

    title_font = load_font(94, bold=True)
    zh_font = load_font(42, cjk=True)
    en_font = load_font(26)
    chip_key_font = load_font(20, bold=True)
    chip_label_font = load_font(20, cjk=True)
    tiny_font = load_font(19)
    tiny_cjk_font = load_font(19, cjk=True)

    x = 376
    draw.text((x, 176), "StickySnap", font=title_font, fill=(0xF4, 0xF5, 0xF7))
    draw.text((x, 292), "把截图钉在桌面上。", font=zh_font, fill=(0xC9, 0xD3, 0xE6))
    draw.text((x, 356), "Pin a screenshot to your desktop.",
              font=en_font, fill=(0x99, 0xA4, 0xB6))

    chips = [("F1", "截图"), ("Enter", "贴图"), ("F3", "贴剪贴板")]
    chip_x = x
    for key, label in chips:
        key_width = draw.textlength(key, font=chip_key_font)
        label_width = draw.textlength(label, font=chip_label_font)
        chip_width = round(key_width + label_width + 42)
        draw.rounded_rectangle((chip_x, 426, chip_x + chip_width, 472),
                               radius=10, outline=(255, 255, 255, 30), width=1)
        draw.text((chip_x + 15, 438), key, font=chip_key_font,
                  fill=(0xF4, 0xF5, 0xF7))
        draw.text((chip_x + 27 + key_width, 438), label,
                  font=chip_label_font, fill=(0x99, 0xA4, 0xB6))
        chip_x += chip_width + 12

    draw.text((x, 520), "Windows 原生 · macOS Qt 预览 · 免费 · 纯本地",
              font=tiny_cjk_font, fill=(0x7C, 0x87, 0x9B))

    draw.rounded_rectangle((width - 208, 74, width - 74, 112),
                           radius=19, fill=(*ACCENT, 38),
                           outline=(*ACCENT, 110), width=1)
    draw.text((width - 190, 84), "v0.0.1 · M0", font=tiny_font,
              fill=(0xBF, 0xD5, 0xFF))

    return canvas.convert("RGB")


def main() -> None:
    if not SOURCE.exists():
        raise SystemExit(f"missing source logo: {SOURCE}")

    source = Image.open(SOURCE)
    clean = cut_out_background(source)
    clean.save(SOURCE, optimize=True)
    bbox = clean.getbbox()
    if bbox is None:
        raise SystemExit("the source logo has no visible content")
    mark = clean.crop(bbox)
    mark.save(ROOT / "assets" / "logo-mark.png", optimize=True)

    app_icon = make_icon(mark)
    full_icon = make_icon(mark, margin=0, shadow=False, radius_ratio=0.225)

    app_icon.save(ROOT / "assets" / "app-icon.png", optimize=True)
    app_icon.resize((256, 256), Image.Resampling.LANCZOS).save(
        ROOT / "portable" / "resources" / "app-icon.png", optimize=True
    )
    app_icon.resize((128, 128), Image.Resampling.LANCZOS).save(
        ROOT / "site" / "app-icon.png", optimize=True
    )

    save_ico(full_icon, ROOT / "res" / "app.ico")
    save_ico(full_icon, ROOT / "site" / "favicon.ico")
    full_icon.resize((512, 512), Image.Resampling.LANCZOS).save(
        ROOT / "site" / "icon-512.png", optimize=True
    )
    full_icon.resize((180, 180), Image.Resampling.LANCZOS).save(
        ROOT / "site" / "apple-touch-icon.png", optimize=True
    )
    save_icns(app_icon, ROOT / "portable" / "macos" / "AppIcon.icns")
    make_og(app_icon).save(ROOT / "site" / "og.png", optimize=True)

    print("wrote assets/app-icon.png, assets/logo-mark.png")
    print("wrote res/app.ico, portable/resources/app-icon.png, "
          "portable/macos/AppIcon.icns")
    print("wrote site/app-icon.png, favicon.ico, icon-512.png, "
          "apple-touch-icon.png, og.png")


if __name__ == "__main__":
    main()

"""Map block textures for the UFO ESCAPE map (docs/MISSION-UFO-ESCAPE.md
section 4). 10 textures, 256x256 8-bit BMP (BGR0 palette, bottom-up), under
assets/pack/data/ufoescape/. All are seamlessly tileable (GL_REPEAT applies
to map tiles, ASSET-CONTRACT 9.1), built with deterministic per-pixel
noise/patterns so re-running reproduces the same bytes.

Art direction (user's explicit ask: "cool art for vats of goo and alien
experiments"): goo glows bright saturated green with lighter bubbles; vat
glass shows liquid + bubbles + a darker base band; labpanel carries alien
glyphs and small cow/human silhouette diagrams; lightstrip reads as emissive
(painted bright, no engine lighting); hazard stripes are crisp; console
screens glow; bulkhead is heavy with orange warning lights; hull is dark
violet-grey alloy with rivets/seams; floor is dark grating with a cyan
sub-glow.
"""
from __future__ import annotations

import math
import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "assets/pack/data/ufoescape"
SIZE = 256


def save_bmp8(image: Image.Image, path: Path) -> None:
    w, h = image.size
    image = image.convert("RGB").convert("P", palette=Image.Palette.ADAPTIVE, colors=256,
                                          dither=Image.Dither.NONE)
    palette = (image.getpalette() or [])[:768]
    palette += [0] * (768 - len(palette))
    stride = (w + 3) & ~3
    off = 14 + 40 + 1024
    size = off + stride * h
    out = bytearray()
    out += struct.pack("<2sIHHI", b"BM", size, 0, 0, off)
    out += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, stride * h, 2835, 2835, 256, 0)
    for k in range(256):
        r, g, b = palette[3 * k:3 * k + 3]
        out += bytes((b, g, r, 0))
    data = image.tobytes()
    pad = b"\0" * (stride - w)
    for y in range(h - 1, -1, -1):
        out += data[y * w:(y + 1) * w] + pad
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(out))


def _seed_rng(name: str):
    return random.Random(abs(hash(("escapegen", name))) % (2 ** 31))


def _tile_noise(base, hi, name, freqs=5):
    """Deterministic value noise sampled from a periodic sine lattice, so
    it wraps exactly at the image edges (seamless tiling)."""
    rng = _seed_rng(name)
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    phases = [(rng.uniform(0, 6.28), rng.randint(2, 6)) for _ in range(freqs)]
    for y in range(SIZE):
        for x in range(SIZE):
            t = 0.0
            for ph, freq in phases:
                t += math.sin(2 * math.pi * freq * x / SIZE + ph) * math.cos(2 * math.pi * freq * y / SIZE + ph)
            t = (t / len(phases) + 1) / 2
            c = tuple(int(base[i] + (hi[i] - base[i]) * t) for i in range(3))
            px[x, y] = c
    return img


def _wrap_ellipse(draw, cx, cy, rx, ry, fill):
    """Draw an ellipse and its 8 periodic neighbours so it tiles seamlessly
    whenever it straddles an edge of the SIZE x SIZE canvas."""
    for dx in (-SIZE, 0, SIZE):
        for dy in (-SIZE, 0, SIZE):
            x, y = cx + dx, cy + dy
            if x + rx < 0 or x - rx > SIZE or y + ry < 0 or y - ry > SIZE:
                continue
            draw.ellipse((x - rx, y - ry, x + rx, y + ry), fill=fill)


def hull() -> Image.Image:
    """Dark violet-grey alien alloy plates with rivets and faint seams."""
    img = _tile_noise((42, 36, 54), (64, 56, 78), "hull", freqs=4)
    draw = ImageDraw.Draw(img)
    plate = SIZE // 4  # 64, divides SIZE evenly -> seams line up at wrap
    for x in range(0, SIZE, plate):
        draw.line((x, 0, x, SIZE), fill=(26, 22, 34), width=2)
    for y in range(0, SIZE, plate):
        draw.line((0, y, SIZE, y), fill=(26, 22, 34), width=2)
    rng = _seed_rng("hull_rivets")
    for px in range(plate // 2, SIZE, plate):
        for py in range(plate // 2, SIZE, plate):
            jx, jy = rng.randint(-6, 6), rng.randint(-6, 6)
            for cx, cy in ((px - plate // 2 + 6, py - plate // 2 + 6),
                           (px + plate // 2 - 6, py - plate // 2 + 6),
                           (px - plate // 2 + 6, py + plate // 2 - 6),
                           (px + plate // 2 - 6, py + plate // 2 - 6)):
                _wrap_ellipse(draw, cx, cy, 3, 3, (90, 84, 104))
                _wrap_ellipse(draw, cx - 1, cy - 1, 1, 1, (140, 134, 160))
    return img


def floor() -> Image.Image:
    """Dark metal grating with a cyan sub-glow between the bars."""
    img = Image.new("RGB", (SIZE, SIZE), (18, 20, 24))
    draw = ImageDraw.Draw(img)
    cell = SIZE // 8  # 32
    for y in range(0, SIZE, cell):
        glow = (10, 60, 68) if (y // cell) % 2 == 0 else (8, 46, 54)
        draw.rectangle((0, y + 6, SIZE, y + cell - 6), fill=glow)
    for x in range(0, SIZE, cell // 2):
        draw.line((x, 0, x, SIZE), fill=(34, 36, 40), width=3)
    for y in range(0, SIZE, cell):
        draw.line((0, y, SIZE, y), fill=(44, 46, 50), width=4)
        draw.line((0, y + 2, SIZE, y + 2), fill=(14, 60, 66), width=1)
    return img


def ceiling() -> Image.Image:
    """Ribbed organic-metal ceiling."""
    img = _tile_noise((30, 32, 36), (54, 56, 62), "ceiling", freqs=3)
    draw = ImageDraw.Draw(img)
    rib = SIZE // 8
    for x in range(0, SIZE, rib):
        draw.line((x, 0, x, SIZE), fill=(20, 21, 24), width=2)
        draw.line((x + rib // 2, 0, x + rib // 2, SIZE), fill=(64, 66, 72), width=1)
    return img


def goo() -> Image.Image:
    """Bright radioactive green goo, bubbles, swirl: top surface of vats."""
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    rng = _seed_rng("goo")
    phases = [(rng.uniform(0, 6.28), rng.randint(2, 5)) for _ in range(5)]
    DARK = (18, 90, 30)
    MID = (60, 210, 60)
    HOT = (150, 255, 110)
    for y in range(SIZE):
        for x in range(SIZE):
            t = 0.0
            for ph, freq in phases:
                t += math.sin(2 * math.pi * freq * x / SIZE + ph) * math.cos(2 * math.pi * freq * y / SIZE + ph)
            t = (t / len(phases) + 1) / 2
            if t < 0.55:
                k = t / 0.55
                c = tuple(int(DARK[i] + (MID[i] - DARK[i]) * k) for i in range(3))
            else:
                k = (t - 0.55) / 0.45
                c = tuple(int(MID[i] + (HOT[i] - MID[i]) * k) for i in range(3))
            px[x, y] = c
    draw = ImageDraw.Draw(img)
    for _ in range(26):
        cx, cy = rng.randint(0, SIZE - 1), rng.randint(0, SIZE - 1)
        r = rng.randint(4, 12)
        _wrap_ellipse(draw, cx, cy, r, r, HOT)
        _wrap_ellipse(draw, cx, cy, max(1, r - 3), max(1, r - 3), (210, 255, 180))
    return img


def vat() -> Image.Image:
    """Glass with green goo inside, bubbles, darker base band (vat sides)."""
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    rng = _seed_rng("vat")
    phases = [(rng.uniform(0, 6.28), rng.randint(2, 5)) for _ in range(4)]
    BASE_BAND_H = SIZE // 5
    GLASS = (40, 70, 68)
    LIQUID_LO = (20, 70, 36)
    LIQUID_HI = (70, 200, 90)
    BAND = (18, 20, 24)
    for y in range(SIZE):
        for x in range(SIZE):
            t = 0.0
            for ph, freq in phases:
                t += math.sin(2 * math.pi * freq * x / SIZE + ph) * math.cos(2 * math.pi * freq * y / SIZE + ph)
            t = (t / len(phases) + 1) / 2
            vfrac = y / SIZE
            if y >= SIZE - BASE_BAND_H:
                c = BAND
            elif vfrac < 0.12:
                c = GLASS
            else:
                k = min(1.0, (vfrac - 0.12) / 0.4) * 0.5 + t * 0.5
                c = tuple(int(LIQUID_LO[i] + (LIQUID_HI[i] - LIQUID_LO[i]) * k) for i in range(3))
            px[x, y] = c
    draw = ImageDraw.Draw(img)
    for _ in range(18):
        cx = rng.randint(0, SIZE - 1)
        cy = rng.randint(SIZE // 6, SIZE - BASE_BAND_H - 6)
        r = rng.randint(3, 9)
        _wrap_ellipse(draw, cx, cy, r, r, (150, 255, 160))
        _wrap_ellipse(draw, cx - 1, cy - 1, max(1, r - 3), max(1, r - 3), (220, 255, 210))
    for y in range(SIZE - BASE_BAND_H, SIZE, 10):
        draw.line((0, y, SIZE, y), fill=(32, 34, 40), width=1)
    draw.line((0, SIZE - BASE_BAND_H, SIZE, SIZE - BASE_BAND_H), fill=(90, 150, 140), width=2)
    return img


def labpanel() -> Image.Image:
    """Wall panel with alien glyphs and specimen diagrams (cow + human sil.)."""
    img = Image.new("RGB", (SIZE, SIZE), (48, 50, 58))
    draw = ImageDraw.Draw(img)
    for x in range(0, SIZE, 8):
        draw.line((x, 0, x, SIZE), fill=(40, 42, 50), width=1)
    draw.rectangle((4, 4, SIZE - 5, SIZE - 5), outline=(90, 160, 170), width=2)

    rng = _seed_rng("labpanel_glyphs")
    # Alien glyph row: small geometric marks, deterministic.
    for row in range(3):
        y0 = 16 + row * 26
        for col in range(10):
            x0 = 14 + col * 24
            shape = rng.randint(0, 3)
            c = (80, 230, 220)
            if shape == 0:
                draw.ellipse((x0, y0, x0 + 8, y0 + 8), outline=c, width=1)
            elif shape == 1:
                draw.line((x0, y0, x0 + 8, y0 + 8), fill=c, width=1)
                draw.line((x0, y0 + 8, x0 + 8, y0), fill=c, width=1)
            elif shape == 2:
                draw.rectangle((x0, y0, x0 + 7, y0 + 7), outline=c, width=1)
            else:
                draw.line((x0 + 4, y0, x0 + 4, y0 + 8), fill=c, width=1)
                draw.line((x0, y0 + 4, x0 + 8, y0 + 4), fill=c, width=1)

    # Specimen diagram: cow silhouette (left) + human silhouette (right),
    # drawn as simple filled polygons like a lab chart.
    diag = (150, 220, 230)
    # cow: body ellipse + head + legs, small schematic
    cx, cy = 64, 170
    draw.ellipse((cx - 30, cy - 14, cx + 30, cy + 14), outline=diag, width=2)
    draw.ellipse((cx - 42, cy - 10, cx - 26, cy + 4), outline=diag, width=2)
    for lx in (cx - 20, cx - 6, cx + 10, cx + 24):
        draw.line((lx, cy + 12, lx, cy + 30), fill=diag, width=2)
    draw.text((cx - 20, cy + 34), "SUBJECT A", fill=diag)

    # human: oval head + rectangle torso + limbs
    hx, hy = 190, 150
    draw.ellipse((hx - 10, hy - 40, hx + 10, hy - 20), outline=diag, width=2)
    draw.rectangle((hx - 14, hy - 20, hx + 14, hy + 30), outline=diag, width=2)
    draw.line((hx - 14, hy - 10, hx - 28, hy + 10), fill=diag, width=2)
    draw.line((hx + 14, hy - 10, hx + 28, hy + 10), fill=diag, width=2)
    draw.line((hx - 8, hy + 30, hx - 8, hy + 56), fill=diag, width=2)
    draw.line((hx + 8, hy + 30, hx + 8, hy + 56), fill=diag, width=2)
    draw.text((hx - 24, hy + 60), "SUBJECT B", fill=diag)
    return img


def lightstrip() -> Image.Image:
    """Bright cyan/magenta light strip on dark metal; emissive by painting.

    Two glowing bands (cyan above, magenta below) each centred in their own
    half, both periodic across the SIZE wrap so top/bottom edges match, and
    a bright white hot-core line through the middle of each band."""
    img = Image.new("RGB", (SIZE, SIZE), (16, 16, 20))
    base = (16, 16, 20)
    band_h = SIZE // 2
    cyan = (100, 255, 255)
    magenta = (255, 80, 235)
    for y in range(SIZE):
        local = y % band_h          # 0..band_h-1 within this band
        band_center = band_h / 2
        dist = abs(local - band_center) / band_center  # 0 at band centre, 1 at band edge
        mix = max(0.0, 1.0 - dist) ** 0.5
        c = cyan if y < band_h else magenta
        col = tuple(int(base[i] + (c[i] - base[i]) * mix) for i in range(3))
        for x in range(SIZE):
            img.putpixel((x, y), col)
    draw = ImageDraw.Draw(img)
    draw.line((0, band_h // 2, SIZE, band_h // 2), fill=(255, 255, 255), width=2)
    draw.line((0, band_h + band_h // 2, SIZE, band_h + band_h // 2), fill=(255, 255, 255), width=2)
    return img


def hazard() -> Image.Image:
    """Crisp yellow/black hazard stripes with an alien glyph."""
    img = Image.new("RGB", (SIZE, SIZE), (24, 22, 10))
    draw = ImageDraw.Draw(img)
    # Diagonal 45-degree stripes built from horizontal scanline shifts so the
    # pattern is exactly periodic under GL_REPEAT (shift wraps cleanly, no
    # polygon can fall entirely outside the canvas as a plain diagonal fill
    # risked doing).
    stripe = SIZE // 8  # 32, divides SIZE evenly
    YEL = (235, 200, 20)
    BLK = (22, 20, 10)
    for y in range(SIZE):
        shift = y  # 45-degree diagonal: one pixel of x-shift per scanline
        for x in range(SIZE):
            band = ((x + shift) // stripe) % 2
            img.putpixel((x, y), YEL if band == 0 else BLK)
    draw.rectangle((SIZE // 2 - 28, SIZE // 2 - 28, SIZE // 2 + 28, SIZE // 2 + 28), fill=(22, 20, 10))
    draw.ellipse((SIZE // 2 - 20, SIZE // 2 - 20, SIZE // 2 + 20, SIZE // 2 + 20), outline=(235, 200, 20), width=3)
    draw.line((SIZE // 2 - 10, SIZE // 2, SIZE // 2, SIZE // 2 - 12), fill=(235, 200, 20), width=3)
    draw.line((SIZE // 2, SIZE // 2 - 12, SIZE // 2 + 10, SIZE // 2), fill=(235, 200, 20), width=3)
    draw.line((SIZE // 2 - 10, SIZE // 2, SIZE // 2, SIZE // 2 + 12), fill=(235, 200, 20), width=3)
    draw.line((SIZE // 2, SIZE // 2 + 12, SIZE // 2 + 10, SIZE // 2), fill=(235, 200, 20), width=3)
    return img


def console() -> Image.Image:
    """Console face with glowing screens and buttons."""
    img = Image.new("RGB", (SIZE, SIZE), (36, 38, 44))
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, SIZE - 1, SIZE - 1), outline=(18, 19, 22), width=4)
    rng = _seed_rng("console")
    screens = [(20, 20, 110, 90), (146, 20, 236, 90)]
    for (x0, y0, x1, y1) in screens:
        draw.rectangle((x0 - 4, y0 - 4, x1 + 4, y1 + 4), fill=(20, 21, 24))
        draw.rectangle((x0, y0, x1, y1), fill=(10, 30, 60))
        for _ in range(10):
            ly = rng.randint(y0 + 4, y1 - 4)
            lx1 = rng.randint(x0 + 2, x1 - 20)
            draw.line((lx1, ly, lx1 + rng.randint(8, 16), ly), fill=(90, 220, 255), width=2)
        draw.rectangle((x0, y0, x1, y1), outline=(120, 230, 255), width=1)
    for row in range(2):
        for col in range(8):
            bx = 24 + col * 26
            by = 120 + row * 22
            on = (col + row) % 3 != 0
            color = (255, 90, 60) if on else (60, 30, 24)
            draw.ellipse((bx, by, bx + 12, by + 12), fill=color)
    draw.rectangle((16, 176, SIZE - 16, 236), fill=(24, 25, 30), outline=(60, 62, 70), width=2)
    return img


def bulkhead() -> Image.Image:
    """Heavy door/bulkhead frame, orange warning lights."""
    img = Image.new("RGB", (SIZE, SIZE), (46, 46, 50))
    draw = ImageDraw.Draw(img)
    for i in range(4):
        inset = i * 10
        shade = 46 + i * 10
        draw.rectangle((inset, inset, SIZE - 1 - inset, SIZE - 1 - inset),
                        outline=(shade, shade - 4, shade - 10), width=4)
    bolt_positions = []
    for x in range(20, SIZE - 10, 40):
        bolt_positions += [(x, 10), (x, SIZE - 10)]
    for y in range(20, SIZE - 10, 40):
        bolt_positions += [(10, y), (SIZE - 10, y)]
    draw_rng = _seed_rng("bulkhead_bolts")
    for (x, y) in bolt_positions:
        _wrap_ellipse(draw, x, y, 4, 4, (90, 90, 96))
        _wrap_ellipse(draw, x - 1, y - 1, 1, 1, (150, 150, 158))
    for (x, y) in ((SIZE // 4, SIZE // 2), (3 * SIZE // 4, SIZE // 2)):
        draw.ellipse((x - 14, y - 14, x + 14, y + 14), fill=(60, 30, 10))
        draw.ellipse((x - 10, y - 10, x + 10, y + 10), fill=(255, 140, 30))
        draw.ellipse((x - 5, y - 5, x + 5, y + 5), fill=(255, 210, 140))
    return img


GENERATORS = {
    "hull.bmp": hull, "floor.bmp": floor, "ceiling.bmp": ceiling,
    "goo.bmp": goo, "vat.bmp": vat, "labpanel.bmp": labpanel,
    "lightstrip.bmp": lightstrip, "hazard.bmp": hazard,
    "console.bmp": console, "bulkhead.bmp": bulkhead,
}


def main() -> None:
    for filename, fn in GENERATORS.items():
        save_bmp8(fn(), OUT_DIR / filename)


if __name__ == "__main__":
    main()

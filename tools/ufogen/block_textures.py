"""Block textures for the UFO FIELD map (docs/MISSION-UFO-FIELD.md section 4).

10 textures, 256x256 8-bit BMP (matching assets/pack/data/starter/*.bmp
sizes/format), under assets/pack/data/ufofield/. All are seamlessly
tileable (GL_REPEAT applies to map tiles, ASSET-CONTRACT 9.1), built with
deterministic per-pixel noise/pattern so re-running reproduces the same
bytes.
"""
from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "assets/pack/data/ufofield"
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
    import random
    return random.Random(abs(hash(name)) % (2 ** 31))


def _tile_noise(base, hi, name, amount=18, cell=1):
    """Deterministic value noise that wraps at the image edges -> seamless
    tiling (sample from periodic sine lattice rather than plain per-pixel
    jitter, which would show a hard seam at the wrap)."""
    rng = _seed_rng(name)
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    import math
    phases = [(rng.uniform(0, 6.28), rng.randint(2, 5)) for _ in range(5)]
    for y in range(SIZE):
        for x in range(SIZE):
            t = 0.0
            for ph, freq in phases:
                t += math.sin(2 * math.pi * freq * x / SIZE + ph) * math.cos(2 * math.pi * freq * y / SIZE + ph)
            t = (t / len(phases) + 1) / 2
            c = tuple(int(base[i] + (hi[i] - base[i]) * t) for i in range(3))
            px[x, y] = c
    return img


def grass() -> Image.Image:
    return _tile_noise((48, 92, 40), (90, 150, 70), "grass")


def dirt() -> Image.Image:
    return _tile_noise((92, 68, 42), (130, 100, 64), "dirt")


def hay() -> Image.Image:
    img = _tile_noise((176, 150, 70), (218, 190, 110), "hay")
    draw = ImageDraw.Draw(img)
    # Lines are drawn width=2 starting at multiples of 8, which tiles
    # exactly (SIZE % 8 == 0) -- but a width=2 line centred ON y=0 only
    # paints y in [0,1], while the matching line at y=SIZE-8 paints
    # [SIZE-8, SIZE-7], leaving the very last row (SIZE-1) unmatched to
    # row 0. Draw the wraparound line explicitly so row 0 and row SIZE-1
    # read as the same periodic stripe.
    for y in range(0, SIZE, 8):
        draw.line((0, y, SIZE, y), fill=(150, 124, 56), width=2)
    draw.line((0, SIZE - 1, SIZE, SIZE - 1), fill=(150, 124, 56))
    return img


def fence() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (70, 55, 38))
    draw = ImageDraw.Draw(img)
    plank_w = SIZE // 8
    rng = _seed_rng("fence")
    for i in range(8):
        x0 = i * plank_w
        shade = rng.randint(-14, 14)
        base = (96 + shade, 76 + shade, 52 + shade)
        draw.rectangle((x0 + 2, 0, x0 + plank_w - 2, SIZE - 1), fill=base)
        for _ in range(6):
            yy = rng.randint(0, SIZE - 1)
            draw.line((x0 + 2, yy, x0 + plank_w - 2, yy), fill=(base[0] - 20, base[1] - 20, base[2] - 20))
    return img


def barn_red() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (138, 36, 30))
    draw = ImageDraw.Draw(img)
    for y in range(0, SIZE, 16):
        draw.line((0, y, SIZE, y), fill=(104, 24, 20), width=2)
    rng = _seed_rng("barn_red")
    for _ in range(300):
        x, y = rng.randint(0, SIZE - 1), rng.randint(0, SIZE - 1)
        draw.point((x, y), fill=(120 + rng.randint(-10, 10), 28, 24))
    return img


def barn_roof() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (58, 58, 62))
    draw = ImageDraw.Draw(img)
    for x in range(0, SIZE, 12):
        draw.line((x, 0, x, SIZE), fill=(42, 42, 46), width=3)
    return img


def trim_white() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (222, 222, 214))
    draw = ImageDraw.Draw(img)
    for y in range(0, SIZE, 20):
        draw.line((0, y, SIZE, y), fill=(190, 190, 182), width=2)
    return img


def ufo_hull() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (178, 182, 188))
    draw = ImageDraw.Draw(img)
    # Panel-line period (32, 64) divides SIZE evenly so the wrap edge lands
    # exactly on a repeat of the pattern (seamless under GL_REPEAT).
    # Horizontal period offset so neither row 0 nor row SIZE-1 sits inside
    # a line stroke -- both edge rows stay plain base colour and therefore
    # match each other across the GL_REPEAT wrap.
    for y in range(16, SIZE, 32):
        draw.line((0, y, SIZE, y), fill=(140, 144, 150), width=2)
    for x in range(0, SIZE, 64):
        draw.line((x, 0, x, SIZE), fill=(150, 154, 160), width=1)
    return img


def ufo_light() -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (20, 20, 24))
    draw = ImageDraw.Draw(img)
    for y in range(SIZE):
        # lime band on top half, magenta band on bottom half, glow blend at centre
        if y < SIZE // 2:
            c = (80, 255, 90)
            fade = abs(y - SIZE // 4) / (SIZE // 4)
        else:
            c = (255, 60, 220)
            fade = abs(y - SIZE * 3 // 4) / (SIZE // 4)
        base = (20, 20, 24)
        mix = max(0.0, 1.0 - fade)
        col = tuple(int(base[i] + (c[i] - base[i]) * mix) for i in range(3))
        draw.line((0, y, SIZE, y), fill=col)
    return img


def ufo_dark() -> Image.Image:
    return _tile_noise((22, 24, 28), (40, 44, 50), "ufo_dark")


GENERATORS = {
    "grass.bmp": grass, "dirt.bmp": dirt, "hay.bmp": hay, "fence.bmp": fence,
    "barn_red.bmp": barn_red, "barn_roof.bmp": barn_roof, "trim_white.bmp": trim_white,
    "ufo_hull.bmp": ufo_hull, "ufo_light.bmp": ufo_light, "ufo_dark.bmp": ufo_dark,
}


def main() -> None:
    for filename, fn in GENERATORS.items():
        save_bmp8(fn(), OUT_DIR / filename)


if __name__ == "__main__":
    main()

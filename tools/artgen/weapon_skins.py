"""Procedural, atlas-aligned weapon/bullet/human skins for the P0 slots that
tools/modelgen's generators reference but that the free pack never shipped
(data/model/weapon/{m92f,de,mac10,m4,grenade}.bmp, data/model/bullet.bmp,
data/model/soldier0.bmp).

Every texture is built *from the same Atlas object the model generator
built*, so each rect lines up with the part that samples it (no name
guessing). Painting is deterministic: a per-rect ``random.Random(seed)``
instance drives pixel noise, where ``seed`` is derived only from the file
name + rect name (never wall-clock/host state), so re-running this module
produces byte-identical BMPs.

BMP rules (ASSET-CONTRACT 1.2): uncompressed 8-bit, 40-byte BITMAPINFOHEADER,
positive (bottom-up) height, full 256-entry BGR0 palette written right after
the info header (no gap).
"""
from __future__ import annotations

import random
import struct
from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.generators import HUMAN_ATLAS, WEAPON_ATLAS_SIZE, _weapon_atlas, bullet
from tools.modelgen.specs import WEAPON_SPECS
from tools.artgen.human_skins import _paint_head_front, make_atlas as _make_human_atlas

ROOT = Path(__file__).resolve().parents[2]
OUT_WEAPON = ROOT / "assets/pack/data/model/weapon"
OUT_MODEL = ROOT / "assets/pack/data/model"

Color = Tuple[int, int, int]

# -- material palettes -------------------------------------------------------
GUNMETAL = (46, 47, 50)
GUNMETAL_HI = (96, 98, 104)
POLYMER = (34, 33, 31)
POLYMER_HI = (70, 66, 60)
WOOD = (92, 61, 35)
WOOD_HI = (139, 97, 56)
BRASS = (156, 122, 46)
BRASS_HI = (206, 169, 86)
OLIVE = (63, 72, 45)
OLIVE_HI = (100, 110, 68)


def _noisy_fill(draw: ImageDraw.ImageDraw, box, base: Color, hi: Color, seed: int,
                 amount: int = 10) -> None:
    """Fill ``box`` (x0,y0,x1,y1) with ``base`` plus deterministic per-pixel
    noise toward ``hi``, and a soft top-left highlight edge."""
    rng = random.Random(seed)
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            t = rng.random()
            c = tuple(int(base[i] + (hi[i] - base[i]) * t * 0.5) for i in range(3))
            jitter = rng.randint(-amount, amount)
            c = tuple(max(0, min(255, c[i] + jitter)) for i in range(3))
            draw.point((x, y), fill=c)
    # Deterministic highlight along the top and left edge (simulated light).
    w, h = x1 - x0, y1 - y0
    edge = max(1, min(w, h) // 8)
    for i in range(edge):
        shade = tuple(int(base[c] + (hi[c] - base[c]) * (1 - i / edge)) for c in range(3))
        draw.line((x0, y0 + i, x1 - 1, y0 + i), fill=shade)
        draw.line((x0 + i, y0, x0 + i, y1 - 1), fill=shade)


def _rect_box(atlas, name: str):
    r = atlas.rects[name]
    return (r.x0, r.y0, r.x1, r.y1)


def _seed(*parts) -> int:
    h = 0
    for p in parts:
        for ch in str(p):
            h = (h * 131 + ord(ch)) & 0xFFFFFFFF
    return h


def paint_weapon_atlas(weapon_key: str) -> Image.Image:
    spec = WEAPON_SPECS[weapon_key]
    atlas = _weapon_atlas()
    size = WEAPON_ATLAS_SIZE
    img = Image.new("RGB", (size, size), GUNMETAL)
    draw = ImageDraw.Draw(img)

    if spec["kind"] == "grenade":
        plan = {
            "body": (OLIVE, OLIVE_HI),
            "misc": (GUNMETAL, GUNMETAL_HI),
            "barrel": (GUNMETAL, GUNMETAL_HI),
            "grip": (OLIVE, OLIVE_HI),
            "stock": (OLIVE, OLIVE_HI),
            "mag": (GUNMETAL, GUNMETAL_HI),
        }
    else:
        grip_mat = (POLYMER, POLYMER_HI) if spec["kind"] in ("rifle", "smg") else (WOOD, WOOD_HI)
        stock_mat = (WOOD, WOOD_HI) if spec["kind"] == "rifle" else (POLYMER, POLYMER_HI)
        plan = {
            "body": (GUNMETAL, GUNMETAL_HI),
            "barrel": (GUNMETAL, GUNMETAL_HI),
            "grip": grip_mat,
            "stock": stock_mat,
            "mag": (GUNMETAL, GUNMETAL_HI),
            "misc": (GUNMETAL, GUNMETAL_HI),
        }

    for name, (base, hi) in plan.items():
        box = _rect_box(atlas, name)
        _noisy_fill(draw, box, base, hi, _seed(weapon_key, name))
    return img


def paint_bullet_atlas() -> Image.Image:
    _, atlas = bullet()
    size = atlas.size
    img = Image.new("RGB", (size, size), BRASS)
    draw = ImageDraw.Draw(img)
    box = _rect_box(atlas, "skin")
    _noisy_fill(draw, box, BRASS, BRASS_HI, _seed("bullet", "skin"), amount=8)
    return img


def paint_soldier0_atlas() -> Image.Image:
    """Third human skin variant (desert tan), same pipeline as
    tools/artgen/human_skins.make_atlas but with its own palette so it
    stays visually distinct from soldier_green/soldier_black."""
    from tools.artgen.human_skins import SOURCE

    source = Image.open(SOURCE).convert("RGB")
    palettes_patch = {
        "tan": ((168, 140, 97), (150, 123, 86), (205, 177, 143), (74, 63, 46)),
    }
    # make_atlas() only knows "green"/"neutral"; reproduce its body with the
    # tan palette inline so the UV alignment logic (HUMAN_ATLAS rects) stays
    # single-sourced from tools.modelgen.generators.
    shirt, trouser, skin, boot = palettes_patch["tan"]
    atlas_img = Image.new("RGB", (256, 256), (45, 42, 35))
    for index, (name, rect) in enumerate(HUMAN_ATLAS.rects.items()):
        box = (rect.x0, rect.y0, rect.x1, rect.y1)
        if name.startswith("head"):
            color = skin if name != "head_top" else (70, 61, 45)
        elif name.startswith("torso") or name.startswith("arm") or name in ("backpack", "cap"):
            color = shirt
        elif name.startswith("leg"):
            color = trouser
        elif name == "hand":
            color = skin
        elif name == "foot":
            color = boot
        else:
            color = (68, 60, 48)
        draw = ImageDraw.Draw(atlas_img)
        if name.startswith("head") or name == "hand":
            hi = tuple(min(255, c + 40) for c in color)
        else:
            hi = tuple(min(255, c + 30) for c in color)
        _noisy_fill(draw, box, color, hi, _seed("soldier0", name, index), amount=6)
        if name == "head_front":
            _paint_head_front(draw, box, skin)
    return atlas_img


def save_bmp8(image: Image.Image, path: Path) -> None:
    """Uncompressed 8-bit BMP, 40-byte info header, full 256-entry BGR0
    palette directly after it, bottom-up rows (ASSET-CONTRACT 1.2)."""
    w, h = image.size
    image = image.convert("RGB").convert(
        "P", palette=Image.Palette.ADAPTIVE, colors=256, dither=Image.Dither.NONE
    )
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


WEAPON_FILES = {
    "m92f": "m92f.bmp",
    "de": "de.bmp",
    "mac10": "mac10.bmp",
    "m4": "m4.bmp",
    "grenade": "grenade.bmp",
}


def main() -> None:
    for key, filename in WEAPON_FILES.items():
        save_bmp8(paint_weapon_atlas(key), OUT_WEAPON / filename)
    save_bmp8(paint_bullet_atlas(), OUT_MODEL / "bullet.bmp")
    save_bmp8(paint_soldier0_atlas(), OUT_MODEL / "soldier0.bmp")


if __name__ == "__main__":
    main()

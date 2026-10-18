"""Compose deterministic, UV-aligned human skin atlases from retained image-gen sources."""
from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image, ImageDraw

from tools.modelgen.generators import HUMAN_ATLAS

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets/sources/human-olive-fabric.png"
OUT_DIR = ROOT / "assets/pack/data/model"


def _fabric(image: Image.Image, size: tuple[int, int], tint: tuple[int, int, int], seed: int) -> Image.Image:
    w, h = image.size
    x = (seed * 137) % max(1, w - min(w, size[0]))
    y = (seed * 251) % max(1, h - min(h, size[1]))
    crop = image.crop((x, y, min(w, x + max(1, size[0])), min(h, y + max(1, size[1]))))
    crop = crop.resize(size, Image.Resampling.LANCZOS)
    return Image.blend(crop, Image.new("RGB", size, tint), 0.68)


def _shade(color: tuple[int, int, int], delta: int) -> tuple[int, int, int]:
    return tuple(max(0, min(255, c + delta)) for c in color)


def _paint_head_front(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int],
                       skin: tuple[int, int, int]) -> None:
    """Low-key, non-cartoon face: hair band, soft brow shading, faint low-
    contrast eye slits, a subtle nose shadow. No eyebrow strokes, no round
    cartoon eyes, no mouth line."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    # Hair band at the top.
    draw.rectangle((x0, y0, x1 - 1, y0 + h // 6), fill=(66, 55, 43))
    # Soft brow band: a thin strip a few shades darker than skin, low contrast.
    brow_y = y0 + h * 4 // 10
    draw.rectangle((x0 + w // 6, brow_y, x1 - 1 - w // 6, brow_y + max(1, h // 24)),
                    fill=_shade(skin, -16))
    # Faint, low-contrast eye slits (thin horizontal strokes, not round eyes).
    eye_y = brow_y + max(2, h // 12)
    eye_w = max(2, w // 7)
    draw.line((x0 + w // 5, eye_y, x0 + w // 5 + eye_w, eye_y), fill=_shade(skin, -46))
    draw.line((x1 - w // 5 - eye_w, eye_y, x1 - w // 5, eye_y), fill=_shade(skin, -46))
    # Faint nose shadow: a very soft single-shade vertical stroke, no outline.
    nose_x = x0 + w // 2
    draw.line((nose_x, eye_y + 2, nose_x, eye_y + h // 5), fill=_shade(skin, 14))


def make_atlas(source: Image.Image, variant: str = "green") -> Image.Image:
    palettes = {
        "green": ((91, 105, 66), (94, 112, 74), (205, 177, 143), (73, 78, 62)),
        "neutral": ((117, 119, 110), (105, 109, 104), (204, 172, 137), (65, 68, 66)),
    }
    shirt, trouser, skin, boot = palettes[variant]
    atlas = Image.new("RGB", (256, 256), (45, 48, 41))
    source = source.convert("RGB")
    for index, (name, rect) in enumerate(HUMAN_ATLAS.rects.items()):
        box = (rect.x0, rect.y0, rect.x1, rect.y1)
        size = (rect.x1 - rect.x0, rect.y1 - rect.y0)
        if name.startswith("head"):
            color = skin if name != "head_top" else (66, 70, 59)
        elif name.startswith("torso") or name.startswith("arm") or name in ("backpack", "cap"):
            color = shirt
        elif name.startswith("leg"):
            color = trouser
        elif name == "hand":
            color = skin
        elif name == "foot":
            color = boot
        else:
            color = (65, 68, 58)
        material = Image.new("RGB",size,color) if name.startswith("head") or name=="hand" else _fabric(source,size,color,index+(0 if variant=="green" else 31))
        if name in ("head_back","head_left","head_right"):
            ImageDraw.Draw(material).rectangle((0,0,size[0]-1,size[1]//3),fill=(66,55,43))
        atlas.paste(material,box[:2])
        draw = ImageDraw.Draw(atlas)
        if name == "head_front":
            _paint_head_front(draw, box, skin)
        elif name == "torso_front":
            draw.rectangle((box[0] + 22, box[1], box[0] + 25, box[1] + 63), fill=(46, 53, 43))
            for yy in (22, 39, 54):
                draw.ellipse((box[0] + 27, box[1] + yy, box[0] + 29, box[1] + yy + 2), fill=(177, 166, 126))
        elif name == "cap":
            draw.rectangle((box[0], box[1] + 14, box[0] + 31, box[1] + 19), fill=(47, 53, 43))
    return atlas


def save_bmp8(image: Image.Image, path: Path) -> None:
    image = image.convert("RGB").convert("P", palette=Image.Palette.ADAPTIVE, colors=256, dither=Image.Dither.NONE)
    palette = image.getpalette()[:768]
    palette += [0] * (768 - len(palette))
    dib = struct.pack("<IiiHHIIiiII", 40, 256, 256, 1, 8, 0, 65536, 2835, 2835, 256, 256)
    palette_bytes = b"".join(bytes((palette[i + 2], palette[i + 1], palette[i], 0)) for i in range(0, 768, 3))
    rows = b"".join(image.crop((0, y, 256, y + 1)).tobytes() for y in range(255, -1, -1))
    offset = 14 + len(dib) + len(palette_bytes)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"BM" + struct.pack("<IHHI", offset + len(rows), 0, 0, offset) + dib + palette_bytes + rows)


def main() -> None:
    source = Image.open(SOURCE).convert("RGB")
    for filename, variant in (("soldier_green.bmp", "green"), ("islam2.bmp", "neutral"),
                              ("soldier_black.bmp", "neutral")):
        save_bmp8(make_atlas(source, variant), OUT_DIR / filename)


if __name__ == "__main__":
    main()

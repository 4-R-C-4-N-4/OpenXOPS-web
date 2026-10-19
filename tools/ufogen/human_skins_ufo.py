"""Human skins for RANCHER + 3 aliens (docs/MISSION-UFO-FIELD.md section 3/E4).

UV-aligned with the existing human rig via tools.modelgen.generators.HUMAN_ATLAS
(same rects every up/arm/leg model samples, ASSET-CONTRACT 5.3). Reuses the
noisy-fill painter and 8-bit BMP writer from tools/artgen/weapon_skins.py so
the output follows the same contract rules as the rest of the pack.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from tools.modelgen.generators import HUMAN_ATLAS
from tools.artgen.weapon_skins import _noisy_fill, _seed, save_bmp8

ROOT = Path(__file__).resolve().parents[2]
OUT_MODEL = ROOT / "assets/pack/data/model"

# name -> (shirt/suit, trousers, skin, boot/trim, is_alien, eyes)
PALETTES = {
    "rancher": dict(shirt=(58, 82, 140), trouser=(46, 64, 112), skin=(196, 150, 112),
                     boot=(92, 60, 30), hat=(196, 168, 92), alien=False),
    "alien_teal": dict(shirt=(170, 176, 182), trouser=(150, 158, 166), skin=(30, 150, 140),
                        boot=(140, 146, 152), hat=None, alien=True),
    "alien_violet": dict(shirt=(230, 128, 30), trouser=(200, 108, 20), skin=(118, 60, 180),
                          boot=(170, 90, 20), hat=None, alien=True),
    "alien_lime": dict(shirt=(20, 20, 22), trouser=(20, 20, 22), skin=(150, 230, 40),
                        boot=(20, 20, 22), hat=None, alien=True, trim=(212, 175, 55)),
}

FILES = {
    "rancher": "rancher.bmp",
    "alien_teal": "alien_teal.bmp",
    "alien_violet": "alien_violet.bmp",
    "alien_lime": "alien_lime.bmp",
}


def _shade(c, d):
    return tuple(max(0, min(255, v + d)) for v in c)


def _paint_alien_eyes(draw: ImageDraw.ImageDraw, box, skin) -> None:
    """Big glossy black almond eyes on head_front, no mouth, no brows."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    eye_y = y0 + h * 2 // 5
    eye_h = h * 2 // 7
    eye_w = w * 7 // 20
    for side in (-1, 1):
        cx = x0 + w // 2 + side * (eye_w // 2 + 2)
        draw.ellipse((cx - eye_w // 2, eye_y, cx + eye_w // 2, eye_y + eye_h), fill=(6, 6, 8))
        # glossy highlight
        draw.ellipse((cx - eye_w // 4, eye_y + 1, cx - eye_w // 4 + 3, eye_y + 4), fill=(210, 220, 230))


def _paint_rancher_head(draw: ImageDraw.ImageDraw, box, skin, hat) -> None:
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    draw.rectangle((x0, y0, x1 - 1, y0 + h // 5), fill=_shade(skin, -40))  # hat brim shadow
    draw.rectangle((x0, y0 + h // 5, x1 - 1, y0 + h // 5 + max(1, h // 14)), fill=hat)  # straw band
    brow_y = y0 + h * 4 // 10
    draw.line((x0 + w // 5, brow_y, x1 - w // 5, brow_y), fill=_shade(skin, -40))
    eye_y = brow_y + max(2, h // 12)
    eye_w = max(2, w // 7)
    draw.line((x0 + w // 5, eye_y, x0 + w // 5 + eye_w, eye_y), fill=_shade(skin, -50))
    draw.line((x1 - w // 5 - eye_w, eye_y, x1 - w // 5, eye_y), fill=_shade(skin, -50))


def make_atlas(name: str) -> Image.Image:
    pal = PALETTES[name]
    shirt, trouser, skin, boot = pal["shirt"], pal["trouser"], pal["skin"], pal["boot"]
    atlas = Image.new("RGB", (256, 256), (40, 40, 42))
    draw = ImageDraw.Draw(atlas)
    for index, (rname, rect) in enumerate(HUMAN_ATLAS.rects.items()):
        box = (rect.x0, rect.y0, rect.x1, rect.y1)
        if rname.startswith("head"):
            color = skin if rname != "head_top" else _shade(skin, -20)
        elif rname.startswith("torso") or rname.startswith("arm") or rname in ("backpack", "cap"):
            color = pal.get("trim", shirt) if (pal.get("alien") and rname == "backpack") else shirt
        elif rname.startswith("leg"):
            color = trouser
        elif rname == "hand":
            color = skin
        elif rname == "foot":
            color = boot
        else:
            color = (50, 50, 52)
        hi = _shade(color, 45)
        _noisy_fill(draw, box, color, hi, _seed(name, rname, index), amount=8)
        if rname == "head_front":
            if pal.get("alien"):
                _paint_alien_eyes(draw, box, skin)
            else:
                _paint_rancher_head(draw, box, skin, pal["hat"])
        elif rname == "torso_front" and name == "alien_lime":
            # gold armour trim seam down the chest.
            trim = pal.get("trim", (212, 175, 55))
            draw.rectangle((box[0] + 22, box[1], box[0] + 25, box[1] + 63), fill=trim)
        elif rname == "torso_front" and name == "rancher":
            draw.rectangle((box[0] + 22, box[1], box[0] + 25, box[1] + 63), fill=(70, 55, 35))
            for yy in (18, 32, 46, 58):
                draw.ellipse((box[0] + 20, box[1] + yy, box[0] + 27, box[1] + yy + 3), fill=(210, 190, 140))
    return atlas


def main() -> None:
    for name, filename in FILES.items():
        save_bmp8(make_atlas(name), OUT_MODEL / filename)


if __name__ == "__main__":
    main()

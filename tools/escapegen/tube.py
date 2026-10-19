"""Specimen tube prop model + texture (docs/MISSION-UFO-ESCAPE.md section 4;
docs/MISSION-UFO-ESCAPE.md section 5.3 addon point). Upright glass cylinder
(8-12 sides) with a metal base and cap. Height about 26 world units at
SMALLOBJECT_SCALE 5 (file height about 5.2), radius about 1.0 file unit
(world ~5). Texture shows green liquid with a floating small cow silhouette
on one side and an alien embryo silhouette on the other.
"""
from __future__ import annotations

import math
import random
from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.primitives import cylinder, merge
from tools.modelgen.uv_atlas import Atlas
from tools.modelgen.xwriter import Mesh, write_x
from tools.artgen.weapon_skins import save_bmp8

ROOT = Path(__file__).resolve().parents[2]
OUT_MODEL = ROOT / "assets/pack/data/model"

TUBE_ATLAS_SIZE = 128
SEGMENTS = 10
RADIUS = 1.0
GLASS_H = 3.6
BASE_H = 0.9
CAP_H = 0.7


def tube_atlas() -> Atlas:
    a = Atlas(TUBE_ATLAS_SIZE)
    a.pack([
        ("glass", 128, 64), ("base", 64, 32), ("cap", 64, 32),
    ])
    return a


def build_tube_mesh() -> Tuple[Mesh, Atlas]:
    atlas = tube_atlas()
    glass_uv = atlas.uv("glass")
    base_uv = atlas.uv("base")
    cap_uv = atlas.uv("cap")

    base_y = BASE_H / 2
    base = cylinder(center=(0, base_y, 0), radius=RADIUS * 1.08, height=BASE_H,
                     segments=SEGMENTS, side_uv=base_uv, cap_uv=base_uv, caps=True)

    glass_y = BASE_H + GLASS_H / 2
    glass = cylinder(center=(0, glass_y, 0), radius=RADIUS, height=GLASS_H,
                      segments=SEGMENTS, side_uv=glass_uv, caps=False)

    cap_y = BASE_H + GLASS_H + CAP_H / 2
    cap = cylinder(center=(0, cap_y, 0), radius=RADIUS * 1.05, height=CAP_H,
                    segments=SEGMENTS, side_uv=cap_uv, cap_uv=cap_uv, caps=True)

    mesh = merge(base, glass, cap)
    return mesh, atlas


def write_tube_model() -> Tuple[str, int]:
    mesh, _ = build_tube_mesh()
    out_path = OUT_MODEL / "tube.x"
    write_x(str(out_path), "tube", [mesh])
    return str(out_path), mesh.poly_count


def _draw_cow_silhouette(draw, cx, cy, scale, color):
    w, h = 26 * scale, 14 * scale
    draw.ellipse((cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2), fill=color)
    draw.ellipse((cx - w / 2 - 8 * scale, cy - h / 2 - 2 * scale, cx - w / 2 + 6 * scale, cy - h / 2 + 10 * scale),
                  fill=color)
    for lx in (cx - w / 2 + 2 * scale, cx - 2 * scale, cx + 6 * scale, cx + w / 2 - 4 * scale):
        draw.line((lx, cy + h / 2, lx, cy + h / 2 + 10 * scale), fill=color, width=max(1, int(2 * scale)))
    draw.line((cx + w / 2, cy, cx + w / 2 + 6 * scale, cy - 6 * scale), fill=color, width=max(1, int(2 * scale)))


def _draw_embryo_silhouette(draw, cx, cy, scale, color):
    # curled alien-embryo shape: large head, small curled body/tail
    draw.ellipse((cx - 16 * scale, cy - 18 * scale, cx + 16 * scale, cy + 6 * scale), fill=color)
    # curled body as a sequence of overlapping shrinking circles
    bx, by = cx, cy + 10 * scale
    r = 10 * scale
    for i in range(5):
        ang = i * 0.9
        bx += math.cos(ang) * r * 0.55
        by += math.sin(ang) * r * 0.55 + 3 * scale
        r *= 0.82
        draw.ellipse((bx - r, by - r, bx + r, by + r), fill=color)
    # large dark eye
    draw.ellipse((cx - 8 * scale, cy - 10 * scale, cx - 1 * scale, cy - 2 * scale), fill=(10, 30, 14))


def paint_tube_atlas() -> Image.Image:
    atlas = tube_atlas()
    img = Image.new("RGB", (TUBE_ATLAS_SIZE, TUBE_ATLAS_SIZE), (20, 22, 24))
    draw = ImageDraw.Draw(img)

    r = atlas.rects["glass"]
    w = r.x1 - r.x0
    h = r.y1 - r.y0
    rng = random.Random(4242)
    LIQ_LO = (14, 64, 30)
    LIQ_HI = (60, 190, 80)
    for y in range(r.y0, r.y1):
        k = (y - r.y0) / h
        row = tuple(int(LIQ_LO[i] + (LIQ_HI[i] - LIQ_LO[i]) * (0.3 + 0.7 * (1 - k))) for i in range(3))
        draw.line((r.x0, y, r.x1, y), fill=row)
    for _ in range(40):
        bx = rng.randint(r.x0 + 2, r.x1 - 2)
        by = rng.randint(r.y0 + 2, r.y1 - 2)
        rr = rng.randint(1, 3)
        draw.ellipse((bx - rr, by - rr, bx + rr, by + rr), fill=(170, 255, 190))

    # left half (one side of the cylinder, u in [0, 0.5) of the rect): cow
    cow_cx = r.x0 + w * 0.25
    cow_cy = r.y0 + h * 0.55
    _draw_cow_silhouette(draw, cow_cx, cow_cy, 0.9, (235, 235, 225))
    # right half: alien embryo
    emb_cx = r.x0 + w * 0.75
    emb_cy = r.y0 + h * 0.45
    _draw_embryo_silhouette(draw, emb_cx, emb_cy, 1.0, (210, 235, 200))

    for y in range(r.y0, r.y1, 10):
        draw.line((r.x0, y, r.x1, y), fill=(40, 120, 100))

    br = atlas.rects["base"]
    draw.rectangle((br.x0, br.y0, br.x1, br.y1), fill=(54, 56, 60))
    for x in range(br.x0, br.x1, 6):
        draw.line((x, br.y0, x, br.y1), fill=(40, 41, 46))
    draw.rectangle((br.x0, br.y0, br.x1, br.y0 + 3), fill=(255, 150, 40))

    cr = atlas.rects["cap"]
    draw.rectangle((cr.x0, cr.y0, cr.x1, cr.y1), fill=(70, 72, 78))
    for x in range(cr.x0, cr.x1, 6):
        draw.line((x, cr.y0, x, cr.y1), fill=(50, 52, 58))
    return img


def main() -> None:
    write_tube_model()
    save_bmp8(paint_tube_atlas(), OUT_MODEL / "tube.bmp")


if __name__ == "__main__":
    main()

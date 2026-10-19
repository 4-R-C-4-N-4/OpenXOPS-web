"""Cow prop model + texture (docs/MISSION-UFO-FIELD.md section 4, addon
small object). ASSET-CONTRACT 6.4: every article uses SMALLOBJECT_SCALE 5.0
at render, so a file built ~2.0 file units tall / ~4.0 file units long
renders at world 10 tall / 20 long, matching the brief."""
from __future__ import annotations

from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.primitives import box, tapered_box, merge
from tools.modelgen.uv_atlas import Atlas
from tools.modelgen.xwriter import Mesh, write_x

ROOT = Path(__file__).resolve().parents[2]
OUT_MODEL = ROOT / "assets/pack/data/model"

COW_ATLAS_SIZE = 128
# File units: SMALLOBJECT_SCALE 5.0 -> world = file * 5. Target world
# body length ~20, height ~10 => file length ~4.0, file height ~2.0.
BODY_LEN = 2.6
BODY_H = 1.1
BODY_W = 0.9
LEG_H = 0.9
HEAD_LEN = 0.7


def cow_atlas() -> Atlas:
    a = Atlas(COW_ATLAS_SIZE)
    a.pack([
        ("body_side", 64, 32), ("body_top", 64, 24), ("body_end", 24, 32),
        ("head", 32, 32), ("leg", 16, 32), ("horn", 8, 16),
    ])
    return a


def build_cow_mesh() -> Tuple[Mesh, Atlas]:
    atlas = cow_atlas()
    all_faces = ("front", "back", "left", "right", "top", "bottom")

    def rect(n):
        return atlas.uv(n)

    body_y = LEG_H + BODY_H / 2
    body = box(center=(0, body_y, 0), size=(BODY_W, BODY_H, BODY_LEN), uv={
        "front": rect("body_end"), "back": rect("body_end"),
        "left": rect("body_side"), "right": rect("body_side"),
        "top": rect("body_top"), "bottom": rect("body_top"),
    })

    neck = tapered_box(
        base_center=(0, body_y + BODY_H * 0.15, -BODY_LEN / 2), base_size=(BODY_W * 0.75, BODY_H * 0.7),
        top_center=(0, LEG_H + HEAD_LEN * 0.55, -BODY_LEN / 2 - HEAD_LEN * 0.35), top_size=(BODY_W * 0.55, BODY_H * 0.55),
        uv={k: rect("head") for k in all_faces})
    head = box(center=(0, LEG_H + HEAD_LEN * 0.55, -BODY_LEN / 2 - HEAD_LEN * 0.75),
               size=(BODY_W * 0.55, HEAD_LEN * 0.8, HEAD_LEN), uv={k: rect("head") for k in all_faces})

    horn_z = -BODY_LEN / 2 - HEAD_LEN * 0.95
    horn_y = LEG_H + HEAD_LEN * 0.9
    horns = []
    for side in (-1, 1):
        horns.append(box(center=(side * BODY_W * 0.22, horn_y, horn_z), size=(0.05, 0.16, 0.05),
                          uv={k: rect("horn") for k in all_faces}))

    legs = []
    leg_half_w, leg_half_d = 0.10, 0.10
    for sx in (-1, 1):
        for sz in (-1, 1):
            x = sx * (BODY_W / 2 - leg_half_w - 0.02)
            z = sz * (BODY_LEN / 2 - leg_half_d - 0.15)
            legs.append(box(center=(x, LEG_H / 2, z), size=(leg_half_w * 2, LEG_H, leg_half_d * 2),
                             uv={k: rect("leg") for k in all_faces}))

    tail = box(center=(0, body_y - BODY_H * 0.1, BODY_LEN / 2 + 0.10), size=(0.05, 0.35, 0.05),
               uv={k: rect("leg") for k in all_faces})

    mesh = merge(body, neck, head, tail, *horns, *legs)
    return mesh, atlas


def write_cow_model() -> Tuple[str, int]:
    mesh, _ = build_cow_mesh()
    out_path = OUT_MODEL / "cow.x"
    write_x(str(out_path), "cow", [mesh])
    return str(out_path), mesh.poly_count


def paint_cow_atlas() -> Image.Image:
    """Black-and-white Holstein patches with a pink nose."""
    atlas = cow_atlas()
    size = COW_ATLAS_SIZE
    WHITE = (235, 235, 230)
    BLACK = (30, 28, 26)
    PINK = (230, 150, 160)
    img = Image.new("RGB", (size, size), WHITE)
    draw = ImageDraw.Draw(img)

    def patches(box_, seed):
        x0, y0, x1, y1 = box_
        import random
        rng = random.Random(seed)
        n = max(2, (x1 - x0) * (y1 - y0) // 180)
        for _ in range(n):
            cx = rng.randint(x0, x1)
            cy = rng.randint(y0, y1)
            rw = rng.randint(4, max(5, (x1 - x0) // 3))
            rh = rng.randint(4, max(5, (y1 - y0) // 3))
            draw.ellipse((cx - rw, cy - rh, cx + rw, cy + rh), fill=BLACK)

    for name in ("body_side", "body_top", "body_end", "head"):
        r = atlas.rects[name]
        box_ = (r.x0, r.y0, r.x1, r.y1)
        draw.rectangle(box_, fill=WHITE)
        patches(box_, hash(name) & 0xFFFF)
    for name in ("leg", "horn"):
        r = atlas.rects[name]
        fill = (210, 205, 195) if name == "leg" else (225, 215, 200)
        draw.rectangle((r.x0, r.y0, r.x1, r.y1), fill=fill)

    # Pink nose on the head rect, lower-centre.
    r = atlas.rects["head"]
    nx0 = r.x0 + (r.x1 - r.x0) // 3
    nx1 = r.x1 - (r.x1 - r.x0) // 3
    ny0 = r.y1 - (r.y1 - r.y0) // 4
    draw.ellipse((nx0, ny0, nx1, r.y1 - 1), fill=PINK)
    return img


def save_bmp8(image: Image.Image, path: Path) -> None:
    import struct

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


def main() -> None:
    write_cow_model()
    save_bmp8(paint_cow_atlas(), OUT_MODEL / "cow.bmp")


if __name__ == "__main__":
    main()

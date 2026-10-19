"""Laser bolt model + colour textures (docs/MISSION-UFO-FIELD.md section 3/E2,
docs/ASSET-CONTRACT.md section 4/6.2 bullet convention).

bullet.x (tools/modelgen/generators.py:bullet) is a cylinder(height 0.25) +
a 0.08-edge nose box, nose toward -Z, total length ~0.33 file units, size
1.0 (parameter.cpp:1434-1436). The spec asks for an elongated bolt "roughly
3-4x longer than bullet.x along the travel axis": laser.x below is a
cylinder(height 1.0) + 0.14-edge nose box, total length ~1.14 (~3.5x), same
nose-toward--Z convention and the same size-1.0 engine scale so it drops in
wherever bullet.x was used (object.cpp:2897 bullet transform).
"""
from __future__ import annotations

from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.primitives import box, cylinder, merge
from tools.modelgen.uv_atlas import Atlas
from tools.modelgen.xwriter import Mesh, write_x

ROOT = Path(__file__).resolve().parents[2]
OUT_MODEL = ROOT / "assets/pack/data/model"

LASER_ATLAS_SIZE = 64
BODY_LEN = 1.00
NOSE_LEN = 0.14
TOTAL_LEN = BODY_LEN + NOSE_LEN  # ~1.14, ~3.5x bullet.x's ~0.33

COLORS = {
    "laser_pink": (0xFF, 0x2F, 0xD6),
    "laser_cyan": (0x2F, 0xF7, 0xFF),
    "laser_lime": (0x9D, 0xFF, 0x2F),
}


def laser_atlas() -> Atlas:
    a = Atlas(LASER_ATLAS_SIZE)
    a.pack([("skin", 64, 64)])
    return a


def build_laser_mesh() -> Tuple[Mesh, Atlas]:
    atlas = laser_atlas()
    uv = {k: atlas.uv("skin") for k in ("front", "back", "left", "right", "top", "bottom")}
    body = cylinder(center=(0, 0, BODY_LEN / 2 - NOSE_LEN / 2), radius=0.045, height=BODY_LEN,
                     segments=6, side_uv=atlas.uv("skin"), cap_uv=atlas.uv("skin"))
    nose = box(center=(0, 0, -TOTAL_LEN / 2 + NOSE_LEN / 2), size=(0.07, 0.07, NOSE_LEN), uv=uv)
    return merge(body, nose), atlas


def write_laser_model() -> Tuple[str, int]:
    mesh, _ = build_laser_mesh()
    out_path = OUT_MODEL / "laser.x"
    write_x(str(out_path), "laser", [mesh])
    return str(out_path), mesh.poly_count


def paint_laser_texture(name: str) -> Image.Image:
    """White-hot core gradient fading out to the spec colour at the edges."""
    color = COLORS[name]
    size = LASER_ATLAS_SIZE
    img = Image.new("RGB", (size, size), color)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    for x in range(size):
        # Horizontal (around-the-cylinder) core band at the texture centre,
        # white-hot in the middle, blending to the pure spec colour at the
        # strip edges -- this is what the side_uv band actually samples.
        dist = abs(x - cx) / cx
        t = max(0.0, 1.0 - dist * 1.7)
        c = tuple(int(color[i] + (255 - color[i]) * t) for i in range(3))
        draw.line((x, 0, x, size - 1), fill=c)
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
    write_laser_model()
    for name in COLORS:
        save_bmp8(paint_laser_texture(name), OUT_MODEL / f"{name}.bmp")


if __name__ == "__main__":
    main()

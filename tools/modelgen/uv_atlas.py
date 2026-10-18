"""UV atlas allocation and wireframe PNG template export.

Per docs/ASSET-CONTRACT.md: texture origin is top-left, v increasing
downward. Atlas rects below are stored as pixel rects (x0,y0,x1,y1) with
that same orientation, and converted to 0..1 UV with v = y/size (no flip
needed since the engine's own convention already has v-down == pixel-row
order, see ASSET-CONTRACT 1.5 and 4 "exact behaviour").
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Tuple


@dataclass
class AtlasRect:
    name: str
    x0: int
    y0: int
    x1: int
    y1: int

    def uv(self, atlas_size: int) -> Tuple[float, float, float, float]:
        return (
            self.x0 / atlas_size,
            self.y0 / atlas_size,
            self.x1 / atlas_size,
            self.y1 / atlas_size,
        )


class Atlas:
    """Simple shelf packer: rects are placed in a fixed grid the caller
    defines explicitly (deterministic, no heuristic packing needed for the
    small rect counts in this project)."""

    def __init__(self, size: int):
        self.size = size
        self.rects: Dict[str, AtlasRect] = {}

    def add(self, name: str, x0: int, y0: int, x1: int, y1: int) -> AtlasRect:
        r = AtlasRect(name, x0, y0, x1, y1)
        self.rects[name] = r
        return r

    def uv(self, name: str) -> Tuple[float, float, float, float]:
        return self.rects[name].uv(self.size)

    def pack(self, items) -> Dict[str, AtlasRect]:
        """Deterministic left-to-right, top-to-bottom shelf packer.

        ``items`` is an ordered list of ``(name, w, h)``. Insertion order is
        preserved exactly (no size-based re-sorting), so re-running the
        generator always produces the same atlas. Raises if an item or a
        row overflows ``self.size`` (keeps layouts honest instead of
        silently clipping)."""
        x = y = row_h = 0
        for name, w, h in items:
            if w > self.size or h > self.size:
                raise ValueError(f"rect {name} ({w}x{h}) exceeds atlas size {self.size}")
            if x + w > self.size:
                x = 0
                y += row_h
                row_h = 0
            if y + h > self.size:
                raise ValueError(f"atlas of size {self.size} overflowed while packing {name}")
            self.add(name, x, y, x + w, y + h)
            x += w
            row_h = max(row_h, h)
        return self.rects


def render_uv_template(atlas: Atlas, out_path: str) -> None:
    """Write a PNG wireframe of every rect in ``atlas`` (own diagram, no
    game art) so an image-gen pass can paint a skin that lines up."""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        _render_uv_template_stub(atlas, out_path)
        return

    size = atlas.size
    img = Image.new("RGB", (size, size), (40, 40, 48))
    draw = ImageDraw.Draw(img)
    colors = [
        (255, 99, 71), (100, 200, 255), (150, 255, 150), (255, 220, 100),
        (220, 140, 255), (255, 160, 200), (160, 255, 220), (255, 255, 255),
    ]
    for i, (name, r) in enumerate(sorted(atlas.rects.items())):
        col = colors[i % len(colors)]
        draw.rectangle([r.x0, r.y0, r.x1 - 1, r.y1 - 1], outline=col, width=1)
        tx, ty = r.x0 + 2, r.y0 + 2
        draw.text((tx, ty), name, fill=col)
    img.save(out_path)


def _render_uv_template_stub(atlas: Atlas, out_path: str) -> None:
    """Minimal pure-stdlib PNG writer fallback (flat-colour rect outlines)
    used only if Pillow is unavailable, so the CLI never hard-fails."""
    import struct
    import zlib

    size = atlas.size
    buf = bytearray(b"\x28\x28\x30" * size * size)  # background grey

    def set_px(x, y, color):
        if 0 <= x < size and 0 <= y < size:
            i = (y * size + x) * 3
            buf[i:i + 3] = bytes(color)

    colors = [
        (255, 99, 71), (100, 200, 255), (150, 255, 150), (255, 220, 100),
        (220, 140, 255), (255, 160, 200),
    ]
    for i, (name, r) in enumerate(sorted(atlas.rects.items())):
        col = colors[i % len(colors)]
        for x in range(r.x0, r.x1):
            set_px(x, r.y0, col)
            set_px(x, r.y1 - 1, col)
        for y in range(r.y0, r.y1):
            set_px(r.x0, y, col)
            set_px(r.x1 - 1, y, col)

    def chunk(tag, data):
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    stride = size * 3
    for y in range(size):
        raw.append(0)
        raw.extend(buf[y * stride:(y + 1) * stride])
    compressed = zlib.compress(bytes(raw), 9)

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    with open(out_path, "wb") as fh:
        fh.write(sig)
        fh.write(chunk(b"IHDR", ihdr))
        fh.write(chunk(b"IDAT", compressed))
        fh.write(chunk(b"IEND", b""))

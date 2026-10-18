#!/usr/bin/env python3
"""Generate our CC0 data/char.dds (font + HUD atlas) and data/title.dds.

Contract: docs/ASSET-CONTRACT.md section 2 ("char.dds: texture font and HUD
atlas"), derived from src/platform/portable/d3dgraphics-portable.cpp
Draw2DTextureFontText (~2400-2463) and the equivalent OpenGL/DirectX backends,
plus every gamemain.cpp call site that samples char.dds as HUD panels/icons.
The exact grid and HUD-cell map is pinned machine-readably in
tools/contract/char_layout.json (already produced from that source audit;
this script only reads it, it does not re-derive the contract).

Glyphs: ASCII 0x20-0x7E rendered from the public-domain Shinonome 8x16 BDF
font (same pinned/checksummed download as tools/fonts/gen_font_atlas.py --
see assets/fonts/README.md and assets/fonts/SHINONOME-LICENSE.txt for
provenance; Public Domain, Electronic Font Open Laboratory). No original
X-operations asset is read or referenced.

HUD cells (0xB0-0xD2, see char_layout.json "hud_cells"): simple original
geometric bevel-panel art drawn with Pillow, since the source fixes only
cell positions/tiling roles/alpha (ASSET-CONTRACT.md Q-2): "design freely,
keeping tile edges continuous."

Usage: python3 tools/artgen/char_atlas.py [--out assets/pack/data/char.dds]
"""
import argparse
import datetime
import hashlib
import json
import os
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "fonts"))
sys.path.insert(0, str(ROOT / "tools" / "artgen"))

import gen_font_atlas as shinonome  # noqa: E402
import dds  # noqa: E402

LAYOUT_PATH = ROOT / "tools" / "contract" / "char_layout.json"
CHAR_OUT = ROOT / "assets" / "pack" / "data" / "char.dds"
TITLE_OUT = ROOT / "assets" / "pack" / "data" / "title.dds"
PREVIEW_CHAR = ROOT / "assets" / "work" / "char_preview.png"
PREVIEW_TITLE = ROOT / "assets" / "work" / "title_preview.png"
PROVENANCE = ROOT / "assets" / "provenance.jsonl"
MANIFEST = ROOT / "assets" / "manifest.json"

SIZE = 512
COLS = ROWS = 16
CELL = SIZE // COLS  # 32


def load_layout():
    return json.loads(LAYOUT_PATH.read_text())


def load_ascii_glyphs():
    """code(0x20-0x7E) -> (bbw=8, bbh=16, rows[16] ints), from the pinned
    Shinonome 8x16 JIS X 0201 BDF (same bytes gen_font_atlas.py downloads)."""
    tar_bytes = shinonome.fetch_tarball()
    import io
    import tarfile
    tf = tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r:gz")
    text = tf.extractfile(shinonome.HALF_BDF).read().decode("ascii", "replace")
    out = {}
    for encoding, bbw, bbh, rows in shinonome.parse_bdf(text):
        if 0x20 <= encoding <= 0x7E:
            out[encoding] = (bbw or 8, bbh or 16, rows)
    return out


def glyph_mask(bbw, bbh, rows, scale=2):
    """Render one BDF glyph (bit rows, MSB-aligned to bbw) to an 'L' mask,
    nearest-neighbour upscaled, no anti-aliasing (keeps bitmap-font crispness)."""
    w, h = bbw, min(16, bbh)
    im = Image.new("L", (w, h), 0)
    px = im.load()
    for y in range(h):
        val = rows[y] if y < len(rows) else 0
        for x in range(w):
            bit = (val >> (bbw - 1 - x)) & 1 if bbw else 0
            px[x, y] = 255 if bit else 0
    return im.resize((w * scale, h * scale), Image.Resampling.NEAREST)


def paint_glyph_cell(atlas, code, bbw, bbh, rows):
    """Paint one 32x32 cell: white glyph in alpha, 1-2px margin (contract
    section 2.1: avoid bilinear bleed into neighbour cells)."""
    col, row = code % COLS, code // COLS
    x0, y0 = col * CELL, row * CELL
    if code == 0x20:
        return  # space: cell must stay fully transparent
    mask = glyph_mask(bbw, bbh, rows, scale=2)  # 16x32 for 8x16 glyphs
    mw, mh = mask.size
    # Centre horizontally, 2px margin top/bottom (cell is 32 tall already).
    ox = x0 + (CELL - mw) // 2
    oy = y0 + max(0, (CELL - mh) // 2)
    glyph_rgba = Image.new("RGBA", mask.size, (255, 255, 255, 0))
    glyph_rgba.putalpha(mask)
    atlas.alpha_composite(glyph_rgba, (ox, oy))


# ---------------------------------------------------------------------------
# HUD cells: original geometric bevel-panel art (no source-mandated look).
# ---------------------------------------------------------------------------

def _dirs_from_purpose(purpose):
    p = purpose.lower()
    return {
        "top": "top" in p,
        "bottom": "bottom" in p,
        "left": "left" in p,
        "right": "right" in p,
    }


def draw_panel_cell(draw, x0, y0, role, purpose):
    """A 32x32 bevelled panel tile: light top/left edge, dark bottom/right
    edge, mid-grey fill -- continuous across neighbouring tiles of the same
    kind (contract: 'edge-continuous art is required across these cells')."""
    fill = (210, 210, 210, 255)
    hi = (255, 255, 255, 255)
    lo = (90, 90, 90, 255)
    x1, y1 = x0 + CELL - 1, y0 + CELL - 1
    draw.rectangle([x0, y0, x1, y1], fill=fill)
    d = _dirs_from_purpose(purpose)

    if role == "corner":
        # Draw the two bevel edges that meet at this corner only.
        if d["top"]:
            draw.line([(x0, y0), (x1, y0)], fill=hi, width=2)
        if d["bottom"]:
            draw.line([(x0, y1), (x1, y1)], fill=lo, width=2)
        if d["left"]:
            draw.line([(x0, y0), (x0, y1)], fill=hi, width=2)
        if d["right"]:
            draw.line([(x1, y0), (x1, y1)], fill=lo, width=2)
    elif role in ("edge",):
        if d["left"]:
            draw.line([(x0, y0), (x0, y1)], fill=hi, width=2)
        if d["right"]:
            draw.line([(x1, y0), (x1, y1)], fill=lo, width=2)
        if not d["left"] and not d["right"]:
            draw.line([(x0, y0), (x1, y0)], fill=hi, width=2)
            draw.line([(x0, y1), (x1, y1)], fill=lo, width=2)
    elif role in ("tile-x", "transition"):
        draw.line([(x0, y0), (x1, y0)], fill=hi, width=2)
        draw.line([(x0, y1), (x1, y1)], fill=lo, width=2)
    elif role == "tile-xy":
        pass  # plain fill, seamless on all 4 sides


def draw_icon_cell(draw, x0, y0, code_dec):
    """Distinct icon glyphs for the 4 icon-role HUD cells."""
    cx, cy = x0 + CELL // 2, y0 + CELL // 2
    white = (255, 255, 255, 255)
    if code_dec in (0xBA, 0xBB):  # ammo icons: filled vs outline bullet-ish shape
        r = 10
        if code_dec == 0xBA:
            draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=white)
        else:
            draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=white, width=3)
    elif code_dec == 0xBD:  # crosshair left bracket "["
        draw.line([(x0 + 10, y0 + 6), (x0 + 6, y0 + 6), (x0 + 6, y0 + 26), (x0 + 10, y0 + 26)],
                  fill=white, width=3, joint="curve")
    elif code_dec == 0xBE:  # crosshair right bracket "]"
        draw.line([(x0 + 22, y0 + 6), (x0 + 26, y0 + 6), (x0 + 26, y0 + 26), (x0 + 22, y0 + 26)],
                  fill=white, width=3, joint="curve")


def paint_hud_cell(atlas, cell):
    code_dec = cell["code_dec"]
    role = cell["role"]
    x0, y0, x1, y1 = cell["rect_px"]
    panel = Image.new("RGBA", (x1 - x0, y1 - y0), (0, 0, 0, 0))
    draw = ImageDraw.Draw(panel)
    if role == "icon":
        draw_icon_cell(draw, 0, 0, code_dec)
        # icons drawn fully opaque white shapes on transparent background
    else:
        draw_panel_cell(draw, 0, 0, role, cell["purpose"])
        # HUD frame cells run to the cell edge at ~0.5 alpha (vertex colour
        # supplies the 0.5; texture alpha stays opaque so it tiles cleanly).
    atlas.alpha_composite(panel, (x0, y0))


# ---------------------------------------------------------------------------

def build_char_atlas():
    layout = load_layout()
    atlas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    glyphs = load_ascii_glyphs()
    for code in range(0x20, 0x7F):
        bbw, bbh, rows = glyphs.get(code, (8, 16, [0] * 16))
        paint_glyph_cell(atlas, code, bbw, bbh, rows)
    for cell in layout["hud_cells"]:
        paint_hud_cell(atlas, cell)
    return atlas


def build_title_logo(w=480, h=80):
    """Our own simple wordmark for the project's title screen, built from the
    same public-domain glyph rasters, upscaled; transparent background."""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    glyphs = load_ascii_glyphs()
    text = "OPENXOPS-WEB"
    scale = 4  # 8x16 -> 32x64
    gw, gh = 8 * scale, 16 * scale
    total_w = gw * len(text)
    ox = max(0, (w - total_w) // 2)
    oy = (h - gh) // 2
    for i, ch in enumerate(text):
        code = ord(ch)
        bbw, bbh, rows = glyphs.get(code, (8, 16, [0] * 16))
        mask = glyph_mask(bbw, bbh, rows, scale=scale)
        glyph_rgba = Image.new("RGBA", mask.size, (235, 235, 245, 0))
        glyph_rgba.putalpha(mask)
        im.alpha_composite(glyph_rgba, (ox + i * gw, oy))
    # Thin underline bar, full width, mimicking a simple logo plate.
    draw.rectangle([ox, oy + gh + 4, ox + total_w, oy + gh + 6], fill=(235, 235, 245, 200))
    return im


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def append_provenance(records):
    with PROVENANCE.open("a") as f:
        for r in records:
            f.write(json.dumps(r) + "\n")


def update_manifest_status(paths):
    if not MANIFEST.exists():
        return
    data = json.loads(MANIFEST.read_text())
    for slot in data["slots"]:
        if slot["path"] in paths:
            slot["status"] = "generated"
    MANIFEST.write_text(json.dumps(data, indent=1) + "\n")


def checkerboard(w, h):
    checker = Image.new("RGBA", (w, h))
    checker.putdata([(150, 150, 150, 255) if (x // 8 + y // 8) % 2 else (210, 210, 210, 255)
                      for y in range(h) for x in range(w)])
    return checker


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(CHAR_OUT))
    ap.add_argument("--title-out", default=str(TITLE_OUT))
    ap.add_argument("--skip-title", action="store_true")
    args = ap.parse_args()

    char_out = Path(args.out)
    char_out.parent.mkdir(parents=True, exist_ok=True)
    atlas = build_char_atlas()
    dds.write_dds(atlas, char_out, fmt="dds_argb4")
    problems = dds.verify_dds(char_out, (SIZE, SIZE), "dds_argb4")
    assert not problems, problems

    PREVIEW_CHAR.parent.mkdir(parents=True, exist_ok=True)
    w, h, px = dds.engine_load(char_out)
    overlay = Image.frombytes("RGBA", (w, h), px)
    Image.alpha_composite(checkerboard(w, h), overlay).convert("RGB").save(PREVIEW_CHAR)

    records = [dict(
        path="data/char.dds", sha256=sha256(char_out),
        generator="Pillow procedural tools/artgen/char_atlas.py",
        prompt=None,
        source=("Shinonome 8x16 BDF (xfonts-shinonome 0.9.11, Public Domain, see "
                "assets/fonts/README.md) for ASCII glyphs; original geometric "
                "Pillow-drawn bevel panels/icons for HUD cells (no original "
                "asset read or referenced; contract: docs/ASSET-CONTRACT.md "
                "section 2, tools/contract/char_layout.json)."),
        license="CC0-1.0", author="OpenXOPS-web contributors",
        date=datetime.date.today().isoformat(), curated_by=None,
    )]
    outputs = ["data/char.dds"]

    if not args.skip_title:
        title_out = Path(args.title_out)
        logo = build_title_logo()
        dds.write_dds(logo, title_out, fmt="dds_argb8")
        problems = dds.verify_dds(title_out, (480, 80), "dds_argb8")
        assert not problems, problems
        w, h, px = dds.engine_load(title_out)
        overlay = Image.frombytes("RGBA", (w, h), px)
        Image.alpha_composite(checkerboard(w, h), overlay).convert("RGB").save(PREVIEW_TITLE)
        records.append(dict(
            path="data/title.dds", sha256=sha256(title_out),
            generator="Pillow procedural tools/artgen/char_atlas.py",
            prompt=None,
            source=("Original wordmark built from the same public-domain "
                    "Shinonome 8x16 glyph rasters, upscaled; no original "
                    "asset read or referenced."),
            license="CC0-1.0", author="OpenXOPS-web contributors",
            date=datetime.date.today().isoformat(), curated_by=None,
        ))
        outputs.append("data/title.dds")

    append_provenance(records)
    update_manifest_status(outputs)
    for r in records:
        print(r["path"], r["sha256"])


if __name__ == "__main__":
    main()

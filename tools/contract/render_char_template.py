#!/usr/bin/env python3
"""Draw a labelled layout template for data/char.dds (texture font + HUD atlas).

The output is our own diagram of the 16x16 cell grid that the engine samples
(see docs/ASSET-CONTRACT.md section 2 and char_layout.json). It contains no
original game art. Artists paint glyphs/HUD pieces into the boxes and then
export a DDS with an alpha channel.

Usage:
    python3 tools/contract/render_char_template.py OUT.png [--size 512] [--layout PATH]
"""
import argparse
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))

ROLE_TAGS = {"corner": "cor", "edge": "edg", "transition": "trn", "tile-x": "t-x", "tile-xy": "txy", "icon": "ico"}

ROLE_COLOURS = {
    "corner": (230, 120, 40, 255),
    "edge": (230, 180, 40, 255),
    "transition": (200, 90, 200, 255),
    "tile-x": (60, 170, 230, 255),
    "tile-xy": (60, 210, 160, 255),
    "icon": (230, 60, 60, 255),
}


def load_font(px):
    for name in ("DejaVuSansMono.ttf", "DejaVuSans.ttf", "LiberationMono-Regular.ttf"):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            continue
    return ImageFont.load_default()


def main(argv=None):
    ap = argparse.ArgumentParser(description="Draw the char.dds layout template (our own diagram).")
    ap.add_argument("out", help="output PNG path")
    ap.add_argument("--size", type=int, default=512, help="template edge in px (default 512)")
    ap.add_argument("--layout", default=os.path.join(HERE, "char_layout.json"))
    args = ap.parse_args(argv)

    with open(args.layout, encoding="utf-8") as fh:
        layout = json.load(fh)

    cols = layout["grid"]["cols"]
    rows = layout["grid"]["rows"]
    size = args.size
    cell = size / cols
    hud = {c["code_dec"]: c for c in layout["hud_cells"]}
    printable = range(32, 127)

    img = Image.new("RGBA", (size, size), (24, 24, 24, 255))
    d = ImageDraw.Draw(img)
    f_small = load_font(max(7, int(cell * 0.22)))
    f_big = load_font(max(9, int(cell * 0.5)))

    for code in range(256):
        cx, cy = code % cols, code // cols
        x0, y0 = int(cx * cell), int(cy * cell)
        x1, y1 = int((cx + 1) * cell) - 1, int((cy + 1) * cell) - 1
        if code in hud:
            col = ROLE_COLOURS.get(hud[code]["role"], (255, 255, 255, 255))
            d.rectangle([x0, y0, x1, y1], fill=col[:3] + (255,), outline=(255, 255, 255, 255))
            d.text((x0 + 2, y0 + 1), f"{code:02X}", fill=(255, 255, 255, 255), font=f_small)
            d.text((x0 + 2, y1 - cell * 0.3), ROLE_TAGS.get(hud[code]["role"], "?"), fill=(255, 255, 255, 255), font=f_small)
        elif code in printable:
            d.rectangle([x0, y0, x1, y1], outline=(120, 120, 120, 255))
            d.text((x0 + 2, y0 + 1), f"{code:02X}", fill=(150, 150, 150, 255), font=f_small)
            if code != 0x20:
                d.text((x0 + cell * 0.3, y0 + cell * 0.3), chr(code), fill=(255, 255, 255, 255), font=f_big)
            else:
                d.text((x0 + 2, y0 + cell * 0.45), "SPC", fill=(150, 150, 150, 255), font=f_small)
        else:
            d.rectangle([x0, y0, x1, y1], outline=(60, 60, 60, 255))
            d.line([x0, y0, x1, y1], fill=(50, 50, 50, 255))

    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
    img.save(args.out)
    print(f"wrote {args.out} ({size}x{size}, {len(hud)} HUD cells, {len(printable)} ASCII cells)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

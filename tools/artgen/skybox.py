"""Procedural, seam-free sky textures for the shared sky box.

The engine wraps one 256x256 BMP onto ``data/sky/sky.x`` (a box visible from
inside; see ASSET-CONTRACT section 7).  ``sky.x`` maps each of its six faces
to a distinct 64x64 quadrant of the image.  A hand-painted image therefore
shows hard seams wherever two box faces meet: the two faces sample two
different parts of the picture at the shared edge.

This module removes that class of defect by construction.  Instead of
painting six independent panels, it *renders a single continuous sky field
over the cube*.  For every texel of every panel it works out the ray
direction from the box centre to that point on the face, then colours the
texel from a smooth function of that direction (a vertical gradient plus
soft periodic cloud noise).  Two texels that sit on the same physical edge
of the cube share the same direction, so they get the same colour and no
seam can appear.  The result is deterministic and contains no copied
content, so it is CC0.

The per-face (u, v) -> direction mapping below is derived from the exact
corner ordering ``box()`` uses in ``tools/modelgen/primitives.py`` together
with the inset (texel-centre) UVs emitted by ``sky()`` in
``tools/modelgen/generators.py``.  Keep the two in lock-step: if the sky box
geometry or its UVs change, re-derive FACE_DIR.
"""
from __future__ import annotations

import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from PIL import Image, ImageDraw


def write_bmp8(p_img: Image.Image, path: Path) -> None:
    """Uncompressed (BI_RGB) bottom-up 8-bit BMP with a full 256-entry
    palette, in the exact layout the engine's LoadBMPTexture expects
    (ASSET-CONTRACT 1.2)."""
    assert p_img.mode == "P"
    w, h = p_img.size
    pal = (p_img.getpalette() or [])[:768]
    pal += [0] * (768 - len(pal))
    stride = (w + 3) & ~3
    off = 14 + 40 + 1024
    size = off + stride * h
    out = bytearray()
    out += struct.pack("<2sIHHI", b"BM", size, 0, 0, off)
    out += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, stride * h,
                       2835, 2835, 256, 0)
    for k in range(256):
        r, g, b = pal[3 * k:3 * k + 3]
        out += bytes((b, g, r, 0))
    data = p_img.tobytes()
    pad = b"\0" * (stride - w)
    for y in range(h - 1, -1, -1):
        out += data[y * w:(y + 1) * w] + pad
    with open(path, "wb") as f:
        f.write(out)
Color = Tuple[int, int, int]


@dataclass(frozen=True)
class SkyPalette:
    fog: Color
    zenith: Color
    ground: Color
    cloud: Color
    cloud_amount: float
    seed: float

# ---------------------------------------------------------------------------
# Geometry: which ray direction does each panel texel correspond to?
#
# The sky box is axis-aligned, centred at the origin in *unit* terms (the
# real centre offset and half-size scale every direction identically, so they
# cancel in the normalised direction).  For a panel, u runs left->right across
# the 64 px and v runs top->bottom, both in [0, 1].  Each entry is the
# unnormalised (dx, dy, dz) as a function of (u, v).
# ---------------------------------------------------------------------------
def _face_dir(u: float, v: float, face: str) -> Tuple[float, float, float]:
    if face == "front":   # z = -1
        return (2 * u - 1, 1 - 2 * v, -1)
    if face == "back":    # z = +1
        return (1 - 2 * u, 1 - 2 * v, 1)
    if face == "left":    # x = -1
        return (-1, 1 - 2 * v, 1 - 2 * u)
    if face == "right":   # x = +1
        return (1, 1 - 2 * v, 2 * u - 1)
    if face == "top":     # y = +1
        return (2 * u - 1, 1, 1 - 2 * v)
    if face == "bottom":  # y = -1
        return (2 * u - 1, -1, 2 * v - 1)
    raise ValueError(face)


# Panel origin (x0, y0) inside the 256x256 image, matching sky()'s atlas.pack.
PANELS: List[Tuple[str, int, int]] = [
    ("front", 0, 0), ("back", 64, 0), ("left", 128, 0), ("right", 192, 0),
    ("top", 0, 64), ("bottom", 64, 64),
]
PANEL = 64


# ---------------------------------------------------------------------------
# A smooth, seamless 3-D value noise (sampled on the unit sphere, so it is
# continuous everywhere and therefore seam-free).
# ---------------------------------------------------------------------------
def _hash3(x: int, y: int, z: int, seed: float) -> float:
    h = math.sin(x * 127.1 + y * 311.7 + z * 74.7 + seed * 13.13) * 43758.5453
    return h - math.floor(h)


def _smoothstep(t: float) -> float:
    return t * t * (3 - 2 * t)


def _clamp01(x: float) -> float:
    return 0.0 if x < 0 else (1.0 if x > 1 else x)


def _value_noise(x: float, y: float, z: float, period: int, seed: float) -> float:
    xi, yi, zi = int(math.floor(x)), int(math.floor(y)), int(math.floor(z))
    xf, yf, zf = x - xi, y - yi, z - zi

    def g(ix: int, iy: int, iz: int) -> float:
        return _hash3(ix % period, iy % period, iz % period, seed)

    c000 = g(xi, yi, zi);      c100 = g(xi + 1, yi, zi)
    c010 = g(xi, yi + 1, zi);  c110 = g(xi + 1, yi + 1, zi)
    c001 = g(xi, yi, zi + 1);  c101 = g(xi + 1, yi, zi + 1)
    c011 = g(xi, yi + 1, zi + 1); c111 = g(xi + 1, yi + 1, zi + 1)
    u = _smoothstep(xf); v = _smoothstep(yf); w = _smoothstep(zf)
    x00 = c000 + (c100 - c000) * u;  x10 = c010 + (c110 - c010) * u
    x01 = c001 + (c101 - c001) * u;  x11 = c011 + (c111 - c011) * u
    y0 = x00 + (x10 - x00) * v;      y1 = x01 + (x11 - x01) * v
    return y0 + (y1 - y0) * w


def _fbm(dx: float, dy: float, dz: float, seed: float) -> float:
    total = 0.0
    amp = 0.5
    freq = 2.0
    for octave in range(3):
        total += amp * _value_noise(dx * freq + 7.3, dy * freq + 3.1,
                                    dz * freq + 5.7, period=16, seed=seed + octave)
        freq *= 2.0
        amp *= 0.5
    return total


def _lerp3(a: Sequence[float], b: Sequence[float], t: float) -> Tuple[float, float, float]:
    return (a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t)


# ---------------------------------------------------------------------------
# Sky palettes.  Fog colours come straight from ASSET-CONTRACT section 7 so
# the horizon blends into the engine fog; zenith / ground / cloud are chosen
# per sky number to give five distinct moods.
# ---------------------------------------------------------------------------
PALETTES: Dict[int, SkyPalette] = {
    n: SkyPalette(**v) for n, v in {
        1: dict(fog=(64, 80, 64), zenith=(46, 92, 168), ground=(58, 66, 54),
                cloud=(238, 242, 248), cloud_amount=0.55, seed=11.0),
        2: dict(fog=(16, 16, 16), zenith=(8, 14, 34), ground=(10, 12, 14),
                cloud=(60, 66, 84), cloud_amount=0.25, seed=23.0),
        3: dict(fog=(0, 16, 32), zenith=(24, 56, 104), ground=(14, 24, 30),
                cloud=(150, 170, 196), cloud_amount=0.45, seed=37.0),
        4: dict(fog=(32, 16, 16), zenith=(74, 32, 54), ground=(26, 16, 18),
                cloud=(178, 120, 120), cloud_amount=0.50, seed=53.0),
        5: dict(fog=(64, 32, 32), zenith=(146, 84, 58), ground=(52, 30, 26),
                cloud=(232, 180, 140), cloud_amount=0.50, seed=71.0),
    }.items()
}


def sky_color(dx: float, dy: float, dz: float, pal: SkyPalette) -> Tuple[float, float, float]:
    n = math.sqrt(dx * dx + dy * dy + dz * dz)
    dx, dy, dz = dx / n, dy / n, dz / n
    elev = dy  # -1 (nadir) .. +1 (zenith)
    fog = pal.fog; zenith = pal.zenith; ground = pal.ground; cloud = pal.cloud
    if elev >= 0:
        t = elev ** 0.75
        base = _lerp3(fog, zenith, t)
        noise = _fbm(dx, dy, dz, pal.seed)
        mask = _smoothstep(_clamp01((noise - 0.46) / 0.24))
        # fade clouds out at the horizon and at the zenith so they never hard-stop
        horizon_fade = _smoothstep(_clamp01(elev / 0.22))
        zenith_fade = 1.0 - _smoothstep(_clamp01((elev - 0.62) / 0.38))
        amount = mask * horizon_fade * zenith_fade * pal.cloud_amount
        return _lerp3(base, cloud, amount)
    t = (-elev) ** 0.7
    return _lerp3(fog, ground, t)


def render_panel(face: str, pal: SkyPalette) -> Image.Image:
    """Render one 64x64 panel whose texels are the cube-projected sky."""
    img = Image.new("RGB", (PANEL, PANEL))
    px = img.load()
    for j in range(PANEL):
        v = (j + 0.5) / PANEL
        for i in range(PANEL):
            u = (i + 0.5) / PANEL
            dx, dy, dz = _face_dir(u, v, face)
            r, g, b = sky_color(dx, dy, dz, pal)
            px[i, j] = (int(round(r)), int(round(g)), int(round(b)))
    return img


def render_sky(number: int, size: int = 256) -> Image.Image:
    """Full 256x256 sky image with the six panels placed per sky()'s atlas."""
    if number not in PALETTES:
        raise ValueError(f"unknown sky number {number}")
    pal = PALETTES[number]
    fog = pal.fog
    img = Image.new("RGB", (size, size), fog)
    for face, x0, y0 in PANELS:
        img.paste(render_panel(face, pal), (x0, y0))
    return img


def save_sky(number: int, path: Path, size: int = 256) -> None:
    rgb = render_sky(number, size)
    # 8-bit palette BMP, matching the contract (bmp8) and the existing writer.
    p = rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT,
                     dither=Image.Dither.FLOYDSTEINBERG)
    write_bmp8(p, path)


def main(argv: Sequence[str] | None = None) -> int:
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=None, help="directory containing sky<N>.bmp (default: assets/pack/data/sky)")
    ap.add_argument("--numbers", default="1", help="comma list of sky numbers to render")
    args = ap.parse_args(argv)
    out = Path(args.out) if args.out else Path(__file__).resolve().parents[2] / "assets/pack/data/sky"
    out.mkdir(parents=True, exist_ok=True)
    for n in (int(x) for x in args.numbers.split(",") if x.strip()):
        p = out / f"sky{n}.bmp"
        save_sky(n, p)
        print(f"wrote {p}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""MOON ROCK throwable weapon/bullet model + texture
(docs/MISSION-UFO-ESCAPE.md section 4, E8; docs/ASSET-CONTRACT.md 6.2 M92F
row). Irregular lumpy low-poly rock (<=60 polys), grey-blue with craters,
sized like a pistol. Grip/muzzle anchors copied verbatim from the M92F
template (docs/ASSET-CONTRACT.md section 6.2 table): grip raw (2,7,57),
muzzle file coords (0.000, 0.114, -0.229).
"""
from __future__ import annotations

import math
import random
from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.xwriter import Mesh, write_x
from tools.artgen.weapon_skins import _noisy_fill, save_bmp8

ROOT = Path(__file__).resolve().parents[2]
OUT_WEAPON = ROOT / "assets/pack/data/model/weapon"

ROCK_SPEC = dict(name="MOON ROCK", template="M92F", size=7.0, grip=(2, 7, 57),
                  muzzle=(0.0, 0.114, -0.229),
                  source="src/parameter.cpp:606-640 (M92F template)")

ROCK_ATLAS_SIZE = 128
SEED = 1101


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _normalize(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return (v[0] / n, v[1] / n, v[2] / n)


def build_rock_mesh() -> Mesh:
    """Irregular lumpy rock: a jittered icosphere-like hull built from a
    low-segment UV sphere lattice whose radius per-vertex is perturbed by
    deterministic noise, so it reads as a lumpy moon rock, not a sphere.
    <=60 polys total (quads count as 1 poly each here, matching the
    engine's face-based poly budget used elsewhere in this project)."""
    rng = random.Random(SEED)
    rings = 5   # latitude rings (excluding poles)
    segs = 8    # longitude segments
    base_r = 0.11

    def radius_at(lat_i, lon_i):
        # deterministic per-vertex jitter + a few low-frequency craters
        seed = (lat_i * 97 + lon_i * 13 + SEED) % (2 ** 31)
        local_rng = random.Random(seed)
        jitter = local_rng.uniform(-0.028, 0.03)
        crater = -0.025 if (lat_i in (1, 3) and lon_i % 3 == 0) else 0.0
        return base_r + jitter + crater

    verts = []  # (lat_i 0..rings+1 incl poles, lon_i) -> point
    top_pole = (0, base_r + 0.02, 0)
    bot_pole = (0, -(base_r + 0.018), 0)
    ring_pts = []
    for li in range(1, rings + 1):
        theta = math.pi * li / (rings + 1)  # 0..pi exclusive of poles
        ring = []
        for lo in range(segs):
            phi = 2 * math.pi * lo / segs
            r = radius_at(li, lo)
            x = r * math.sin(theta) * math.cos(phi)
            y = r * math.cos(theta)
            z = r * math.sin(theta) * math.sin(phi)
            ring.append((x, y, z))
        ring_pts.append(ring)

    mesh = Mesh()
    center = (0.0, 0.0, 0.0)

    def add_tri(p0, p1, p2, uv0, uv1, uv2):
        n = _cross(_sub(p1, p0), _sub(p2, p0))
        fc = ((p0[0] + p1[0] + p2[0]) / 3, (p0[1] + p1[1] + p2[1]) / 3, (p0[2] + p1[2] + p2[2]) / 3)
        if sum(n[i] * (fc[i] - center[i]) for i in range(3)) < 0:
            p1, p2 = p2, p1
            uv1, uv2 = uv2, uv1
        mesh.add_tri_face(p0, uv0, p1, uv1, p2, uv2)

    def add_quad(p0, p1, p2, p3, uv0, uv1, uv2, uv3):
        n = _cross(_sub(p1, p0), _sub(p2, p0))
        fc = tuple(sum(p[i] for p in (p0, p1, p2, p3)) / 4 for i in range(3))
        if sum(n[i] * (fc[i] - center[i]) for i in range(3)) < 0:
            p0, p1, p2, p3 = p3, p2, p1, p0
            uv0, uv1, uv2, uv3 = uv3, uv2, uv1, uv0
        mesh.add_quad_face(p0, uv0, p1, uv1, p2, uv2, p3, uv3)

    rock_uv = (0.05, 0.05, 0.95, 0.95)  # single rect, whole atlas is rock stone
    u0, v0, u1, v1 = rock_uv

    # top cap: pole triangles to first ring
    for lo in range(segs):
        p1 = ring_pts[0][lo]
        p2 = ring_pts[0][(lo + 1) % segs]
        add_tri(top_pole, p1, p2, ((u0 + u1) / 2, v0), (u0, v1), (u1, v1))

    # middle bands: quads between consecutive rings
    for li in range(rings - 1):
        for lo in range(segs):
            a = ring_pts[li][lo]
            b = ring_pts[li][(lo + 1) % segs]
            c = ring_pts[li + 1][(lo + 1) % segs]
            d = ring_pts[li + 1][lo]
            add_quad(a, b, c, d, (u0, v0), (u1, v0), (u1, v1), (u0, v1))

    # bottom cap
    for lo in range(segs):
        p1 = ring_pts[-1][lo]
        p2 = ring_pts[-1][(lo + 1) % segs]
        add_tri(bot_pole, p2, p1, ((u0 + u1) / 2, v1), (u1, v0), (u0, v0))

    return mesh


def write_rock_model() -> Tuple[str, int]:
    mesh = build_rock_mesh()
    out_path = OUT_WEAPON / "rock.x"
    write_x(str(out_path), "rock", [mesh])
    return str(out_path), mesh.poly_count


def paint_rock_atlas() -> Image.Image:
    """Grey-blue rock stone with craters, deterministic noise."""
    img = Image.new("RGB", (ROCK_ATLAS_SIZE, ROCK_ATLAS_SIZE), (74, 82, 96))
    draw = ImageDraw.Draw(img)
    _noisy_fill(draw, (0, 0, ROCK_ATLAS_SIZE, ROCK_ATLAS_SIZE), (58, 66, 82), (108, 118, 134),
                seed=SEED, amount=24)
    rng = random.Random(SEED + 7)
    for _ in range(14):
        cx = rng.randint(10, ROCK_ATLAS_SIZE - 10)
        cy = rng.randint(10, ROCK_ATLAS_SIZE - 10)
        r = rng.randint(4, 14)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(40, 46, 58))
        draw.ellipse((cx - r + 2, cy - r + 2, cx + r - 2, cy + r - 2), fill=(52, 60, 74))
        hl = rng.randint(2, 4)
        draw.ellipse((cx - r // 2, cy - r // 2, cx - r // 2 + hl, cy - r // 2 + hl), fill=(130, 140, 155))
    for _ in range(40):
        x, y = rng.randint(0, ROCK_ATLAS_SIZE - 1), rng.randint(0, ROCK_ATLAS_SIZE - 1)
        draw.point((x, y), fill=(120, 128, 142))
    return img


def main() -> None:
    write_rock_model()
    save_bmp8(paint_rock_atlas(), OUT_WEAPON / "rock.bmp")


if __name__ == "__main__":
    main()

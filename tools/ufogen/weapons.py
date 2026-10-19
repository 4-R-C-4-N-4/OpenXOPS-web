"""Weapon models + skins for lever/zap/photon/plasma (docs/MISSION-UFO-FIELD.md
section 3/E1, docs/ASSET-CONTRACT.md section 6).

Muzzle/grip anchors are copied verbatim from the engine hold offsets of the
template weapon each alien/ranger gun is "based on" (docs/MISSION-UFO-FIELD.md
section 3 table), using the same (mx,my,mz)/10 grip and derived muzzle-file
numbers already tabulated in docs/ASSET-CONTRACT.md section 6.2:

    LEVER   <- M1    size 7.7  grip (8,7,45)   muzzle (0.000, 0.052, -0.636)
    ZAP     <- DE    size 11   grip (2,11,59)  muzzle (0.000, 0.027, -0.145)
    PHOTON  <- P90   size 6.5  grip (8,10,30)  muzzle (0.000, 0.031, -0.446)
    PLASMA  <- M249  size 7.5  grip (8,12,44)  muzzle (-0.013, 0.013, -0.773)

Meshes reuse tools/modelgen's primitives/xwriter so the output is the same
engine-valid .x text subset as the rest of modelgen. Textures reuse the
noisy-fill painter style from tools/artgen/weapon_skins.py so the BMPs follow
the same 8-bit/BGR0-palette contract rules.
"""
from __future__ import annotations

import struct
from pathlib import Path
from typing import Tuple

from PIL import Image, ImageDraw

from tools.modelgen.primitives import box, tapered_box, cylinder, merge
from tools.modelgen.uv_atlas import Atlas
from tools.modelgen.xwriter import Mesh, write_x
from tools.artgen.weapon_skins import _noisy_fill, _seed, save_bmp8

ROOT = Path(__file__).resolve().parents[2]
OUT_WEAPON = ROOT / "assets/pack/data/model/weapon"

# name -> (template, size, grip(mx,my,mz) raw, muzzle file coords, kind)
UFO_WEAPON_SPECS = {
    "lever": dict(name="LEVER 30-30", template="M1", size=7.7, grip=(8, 7, 45),
                   muzzle=(0.0, 0.052, -0.636), kind="rifle",
                   source="src/parameter.cpp:1166-1200 (M1 template)"),
    "zap": dict(name="ZAP PISTOL", template="DE", size=11.0, grip=(2, 11, 59),
                muzzle=(0.0, 0.027, -0.145), kind="pistol",
                source="src/parameter.cpp:676-710 (DE template)"),
    "photon": dict(name="PHOTON SMG", template="P90", size=6.5, grip=(8, 10, 30),
                   muzzle=(0.0, 0.031, -0.446), kind="smg",
                   source="src/parameter.cpp:781-815 (P90 template)"),
    "plasma": dict(name="PLASMA RIFLE", template="M249", size=7.5, grip=(8, 12, 44),
                   muzzle=(-0.013, 0.013, -0.773), kind="rifle",
                   source="src/parameter.cpp:921-955 (M249 template)"),
}

ACCENT_COLOR = {
    "lever": (210, 210, 220),   # scope body (neutral metal; lens drawn separately)
    "zap": (255, 60, 210),      # pink emitter tip
    "photon": (60, 230, 230),   # cyan energy cell
    "plasma": (170, 255, 60),   # lime coils
}

WEAPON_ATLAS_SIZE = 128


def _atlas() -> Atlas:
    a = Atlas(WEAPON_ATLAS_SIZE)
    a.pack([
        ("body", 64, 32), ("barrel", 48, 16), ("grip", 24, 32),
        ("stock", 32, 16), ("mag", 16, 32), ("misc", 16, 16),
        ("accent", 32, 16), ("scope", 24, 16),
    ])
    return a


def _rect(atlas: Atlas, name: str):
    return atlas.uv(name)


def build_weapon_mesh(key: str) -> Tuple[Mesh, Atlas]:
    """Build an engine-valid low-poly weapon mesh with a part shaped after
    the mission brief (docs/MISSION-UFO-FIELD.md section 3), anchored at the
    derived template muzzle/grip so Resource/object.cpp placement lines up."""
    spec = UFO_WEAPON_SPECS[key]
    atlas = _atlas()
    all_faces = ("front", "back", "left", "right", "top", "bottom")

    def rect(n):
        return _rect(atlas, n)

    muzzle = spec["muzzle"]
    kind = spec["kind"]
    extent = -muzzle[2] if muzzle[2] < -0.01 else 0.5
    barrel_y = muzzle[1]

    body_len = extent * (0.65 if kind == "pistol" else 0.42)
    barrel_len = extent - body_len
    barrel_r = {"pistol": 0.028, "smg": 0.022, "rifle": 0.020}[kind]
    if key == "lever":
        barrel_r = 0.024  # chunky octagonal-feel barrel (approximated: hex cylinder)

    gripb = box(center=(0, barrel_y - 0.12, 0.05), size=(0.08, 0.20, 0.11),
                uv={k: rect("grip") for k in all_faces})
    body = box(center=(0, barrel_y - 0.02, -body_len / 2), size=(0.09, 0.11, body_len),
               uv={k: rect("body") for k in all_faces})
    segs = 8 if key == "lever" else 6
    barrel = cylinder(center=(0, 0, 0), radius=barrel_r, height=barrel_len,
                       segments=segs, side_uv=rect("barrel"))
    barrel.verts = [(x, barrel_y - z, -body_len - barrel_len / 2 + y) for x, y, z in barrel.verts]
    parts = [gripb, body, barrel]

    if key == "lever":
        # wood stock+forend (long, tapered toward the butt), tube magazine
        # under the barrel, a lever loop under the receiver, scope on top.
        stock = tapered_box(
            base_center=(0, 0.01, body_len * 0.0), base_size=(0.06, 0.07),
            top_center=(0, -0.015, 0.42), top_size=(0.05, 0.065),
            uv={k: rect("stock") for k in all_faces})
        forend = tapered_box(
            base_center=(0, barrel_y - 0.015, -body_len * 0.6), base_size=(0.05, 0.05),
            top_center=(0, barrel_y - 0.015, -body_len - barrel_len * 0.55), top_size=(0.04, 0.04),
            uv={k: rect("stock") for k in all_faces})
        tube = cylinder(center=(0, barrel_y - 0.045, -body_len - barrel_len * 0.5),
                         radius=0.016, height=barrel_len * 0.95, segments=6,
                         side_uv=rect("mag"))
        tube.verts = [(x, y, z) for x, y, z in tube.verts]
        # cylinder() axis is Y; rotate into Z by the same swap used for the barrel.
        tube.verts = [(x, barrel_y - 0.045 - (z - (-body_len - barrel_len * 0.5)),
                        (-body_len - barrel_len * 0.5) + y) for x, y, z in tube.verts]
        loop = box(center=(0, barrel_y - 0.22, 0.06), size=(0.018, 0.09, 0.05),
                   uv={k: rect("misc") for k in all_faces})
        scope = cylinder(center=(0, barrel_y + 0.09, -body_len * 0.55), radius=0.022,
                          height=0.16, segments=6, side_uv=rect("scope"), caps=True)
        scope.verts = [(x, z - (-body_len * 0.55) + barrel_y + 0.09, barrel_y + 0.09 - y)
                        for x, y, z in scope.verts]
        parts += [stock, forend, tube, loop, scope]
    elif key == "zap":
        # chunky ray-gun pistol with a pink emitter tip.
        tip = cylinder(center=(0, barrel_y, -body_len - barrel_len - 0.03), radius=0.045,
                        height=0.06, segments=6, side_uv=rect("accent"), cap_uv=rect("accent"))
        tip.verts = [(x, barrel_y - z, -body_len - barrel_len - 0.03 + y) for x, y, z in tip.verts]
        fin = box(center=(0, barrel_y + 0.04, -body_len * 0.4), size=(0.10, 0.03, body_len * 0.5),
                   uv={k: rect("misc") for k in all_faces})
        parts += [tip, fin]
    elif key == "photon":
        # compact SMG with a cyan energy cell under the receiver.
        cell = box(center=(0, barrel_y - 0.16, -body_len * 0.35), size=(0.05, 0.10, 0.09),
                   uv={k: rect("accent") for k in all_faces})
        mag = tapered_box(
            base_center=(0, barrel_y - 0.08, -body_len * 0.1), base_size=(0.03, 0.05),
            top_center=(0, barrel_y - 0.24, -body_len * 0.1), top_size=(0.022, 0.03),
            uv={k: rect("mag") for k in all_faces})
        parts += [cell, mag]
    elif key == "plasma":
        # bulky rifle with lime coil rings wrapped around the barrel.
        stock = tapered_box(
            base_center=(0, barrel_y, 0.0), base_size=(0.07, 0.07),
            top_center=(0, barrel_y - 0.02, 0.30), top_size=(0.05, 0.05),
            uv={k: rect("stock") for k in all_faces})
        parts.append(stock)
        for i in range(3):
            frac = 0.25 + i * 0.25
            coil = cylinder(center=(0, 0, 0), radius=barrel_r * 1.8, height=0.025,
                             segments=6, side_uv=rect("accent"), caps=False)
            z0 = -body_len - barrel_len * frac
            coil.verts = [(x, barrel_y - z, z0 + y) for x, y, z in coil.verts]
            parts.append(coil)

    mesh = merge(*parts)
    return mesh, atlas


def write_weapon_model(key: str) -> Tuple[str, int]:
    mesh, _ = build_weapon_mesh(key)
    out_path = OUT_WEAPON / f"{key}.x"
    write_x(str(out_path), key, [mesh])
    return str(out_path), mesh.poly_count


def paint_weapon_atlas(key: str) -> Image.Image:
    atlas = _atlas()
    spec = UFO_WEAPON_SPECS[key]
    GUNMETAL = (46, 47, 50)
    GUNMETAL_HI = (96, 98, 104)
    WOOD = (92, 61, 35)
    WOOD_HI = (139, 97, 56)
    POLYMER = (34, 33, 31)
    POLYMER_HI = (70, 66, 60)
    accent = ACCENT_COLOR[key]
    accent_hi = tuple(min(255, c + 60) for c in accent)

    img = Image.new("RGB", (WEAPON_ATLAS_SIZE, WEAPON_ATLAS_SIZE), GUNMETAL)
    draw = ImageDraw.Draw(img)

    if key == "lever":
        plan = {
            "body": (GUNMETAL, GUNMETAL_HI), "barrel": (GUNMETAL, GUNMETAL_HI),
            "grip": (WOOD, WOOD_HI), "stock": (WOOD, WOOD_HI),
            "mag": (GUNMETAL, GUNMETAL_HI), "misc": (GUNMETAL, GUNMETAL_HI),
            "accent": (accent, accent_hi), "scope": ((20, 20, 24), (60, 60, 68)),
        }
    else:
        plan = {
            "body": (GUNMETAL, GUNMETAL_HI), "barrel": (GUNMETAL, GUNMETAL_HI),
            "grip": (POLYMER, POLYMER_HI), "stock": (POLYMER, POLYMER_HI),
            "mag": (GUNMETAL, GUNMETAL_HI), "misc": (GUNMETAL, GUNMETAL_HI),
            "accent": (accent, accent_hi), "scope": (GUNMETAL, GUNMETAL_HI),
        }

    for name, (base, hi) in plan.items():
        rect = atlas.rects[name]
        box_ = (rect.x0, rect.y0, rect.x1, rect.y1)
        _noisy_fill(draw, box_, base, hi, _seed(key, name))

    if key == "lever":
        # scope lens: a bright inset dot on the scope rect.
        r = atlas.rects["scope"]
        cx, cy = (r.x0 + r.x1) // 2, (r.y0 + r.y1) // 2
        draw.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), fill=(140, 200, 230))
    return img


def main() -> None:
    for key in UFO_WEAPON_SPECS:
        write_weapon_model(key)
        save_bmp8(paint_weapon_atlas(key), OUT_WEAPON / f"{key}.bmp")


if __name__ == "__main__":
    main()

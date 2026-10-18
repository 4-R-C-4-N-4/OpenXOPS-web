"""Per-slot mesh builders.

Every function here returns ``(Mesh, Atlas)``. All humanoid parts share one
256x256 atlas (``HUMAN_ATLAS``) so any skin fits any part, per
docs/ASSET-CONTRACT.md 5.3 ("give up0-5, arm00-02, leg and the walk/run sets
one common UV layout"). Weapons/bullet/sky each get their own small atlas.

Coordinates follow docs/ASSET-CONTRACT.md 4.2/5.3: left-handed, +Y up,
model front = -Z, centred on x=z=0 unless noted. See specs.py for the exact
source citations behind every scale/origin number used below.
"""
from __future__ import annotations

import math
from typing import Tuple

from .primitives import (
    box, tapered_box, cylinder, merge, flip_mesh, translate, rotate_x_about,
)
from .uv_atlas import Atlas
from .xwriter import Mesh

# -- Shared human atlas -------------------------------------------------
HUMAN_ATLAS_SIZE = 256


def _build_human_atlas() -> Atlas:
    a = Atlas(HUMAN_ATLAS_SIZE)
    a.pack([
        ("torso_front", 48, 64), ("torso_back", 48, 64),
        ("torso_left", 24, 64), ("torso_right", 24, 64), ("torso_top", 48, 24),
        ("head_front", 32, 32), ("head_back", 32, 32),
        ("head_left", 24, 32), ("head_right", 24, 32), ("head_top", 32, 24),
        ("backpack", 48, 32), ("cap", 32, 20),
        ("arm_upper", 64, 24), ("arm_lower", 48, 20), ("hand", 24, 24),
        ("leg_upper", 64, 32), ("leg_lower", 48, 24), ("foot", 32, 16),
    ])
    return a


HUMAN_ATLAS = _build_human_atlas()


def _r(name):
    return HUMAN_ATLAS.uv(name)


# -- Humans ---------------------------------------------------------------
# File-y limits derived in docs/ASSET-CONTRACT.md 5.3 from the bullet
# collision cylinders (objectmanager.cpp:755-787) and render scale 9.4
# (object.cpp:2154-2159,178):
UP_HIP_Y = 1.223
UP_SHOULDER_Y = 1.809
UP_HEAD_TOP_Y = 2.277
TORSO_HALF_W = 0.20   # <= radius limit 0.255
HEAD_HALF_W = 0.14    # <= radius limit 0.16


def human_upper(backpack: bool = False, cap: bool = False, label: str = "") -> Tuple[Mesh, Atlas]:
    torso_h = UP_SHOULDER_Y - UP_HIP_Y
    torso = box(
        center=(0, (UP_HIP_Y + UP_SHOULDER_Y) / 2, 0),
        size=(TORSO_HALF_W * 2, torso_h, TORSO_HALF_W * 2 * 0.6),
        uv={
            "front": _r("torso_front"), "back": _r("torso_back"),
            "left": _r("torso_left"), "right": _r("torso_right"),
            "top": _r("torso_top"),
        },
    )
    head_h = UP_HEAD_TOP_Y - UP_SHOULDER_Y
    head = box(
        center=(0, (UP_SHOULDER_Y + UP_HEAD_TOP_Y) / 2, 0),
        size=(HEAD_HALF_W * 2, head_h, HEAD_HALF_W * 2),
        uv={
            "front": _r("head_front"), "back": _r("head_back"),
            "left": _r("head_left"), "right": _r("head_right"),
            "top": _r("head_top"),
        },
    )
    parts = [torso, head]
    if backpack:
        bp = box(
            center=(0, (UP_HIP_Y + UP_SHOULDER_Y) / 2, TORSO_HALF_W * 0.6 + 0.07),
            size=(TORSO_HALF_W * 1.4, torso_h * 0.75, 0.14),
            uv={k: _r("backpack") for k in ("front", "back", "left", "right", "top", "bottom")},
        )
        parts.append(bp)
    if cap:
        cp = box(
            center=(0, UP_HEAD_TOP_Y + 0.03, -0.01),
            size=(HEAD_HALF_W * 2.1, 0.07, HEAD_HALF_W * 2.1),
            uv={k: _r("cap") for k in ("front", "back", "left", "right", "top", "bottom")},
        )
        parts.append(cp)
    return merge(*parts), HUMAN_ATLAS


# Arm pivot = origin (shoulder line), per ASSET-CONTRACT 5.3.
ARM_SHOULDER_HALF_W = 0.14
ARM_UPPER_LEN = 0.30
ARM_LOWER_LEN = 0.28
ARM_HAND_LEN = 0.12


def _one_arm(side: float, spacing: float) -> Mesh:
    """``side`` = -1 (left) or +1 (right). Arm extends along -Z (forward,
    pitch 0) from the shoulder pivot at the origin's y=0 plane."""
    x = side * spacing
    upper = tapered_box(
        base_center=(x, -0.02, 0), base_size=(0.09, 0.09),
        top_center=(x, -0.02, -ARM_UPPER_LEN), top_size=(0.07, 0.07),
        uv={k: _r("arm_upper") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    lower = tapered_box(
        base_center=(x, -0.02, -ARM_UPPER_LEN), base_size=(0.07, 0.07),
        top_center=(x, -0.02, -ARM_UPPER_LEN - ARM_LOWER_LEN), top_size=(0.055, 0.055),
        uv={k: _r("arm_lower") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    hand = box(
        center=(x, -0.02, -ARM_UPPER_LEN - ARM_LOWER_LEN - ARM_HAND_LEN / 2),
        size=(0.06, 0.05, ARM_HAND_LEN),
        uv={k: _r("hand") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    return merge(upper, lower, hand)


def human_arms(pose: str = "down", label: str = "") -> Tuple[Mesh, Atlas]:
    # pose only changes the shoulder spacing (a crude stand-in for
    # open-hand vs. gripping poses); the mesh itself always points along
    # -Z, matching the shoulder-pivot convention (ASSET-CONTRACT 5.2-5.3).
    spacing = {"down": ARM_SHOULDER_HALF_W, "twohand": ARM_SHOULDER_HALF_W * 0.6,
               "onehand": ARM_SHOULDER_HALF_W}.get(pose, ARM_SHOULDER_HALF_W)
    left = _one_arm(-1, spacing)
    right = _one_arm(1, spacing)
    return merge(left, right), HUMAN_ATLAS


# Legs: file-y 0.022 (soles) .. 1.189 (hip), ASSET-CONTRACT 5.3.
LEG_SOLE_Y = 0.022
LEG_HIP_Y = 1.189
LEG_KNEE_Y = 0.55
LEG_HALF_W = 0.11  # <= 0.29 radius limit
LEG_SPACING = 0.14


def _one_leg(side: float) -> Mesh:
    x = side * LEG_SPACING
    thigh = tapered_box(
        base_center=(x, LEG_HIP_Y, 0), base_size=(LEG_HALF_W * 2, LEG_HALF_W * 2 * 0.8),
        top_center=(x, LEG_KNEE_Y, 0), top_size=(LEG_HALF_W * 1.6, LEG_HALF_W * 1.6 * 0.8),
        uv={k: _r("leg_upper") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    shin = tapered_box(
        base_center=(x, LEG_KNEE_Y, 0), base_size=(LEG_HALF_W * 1.6, LEG_HALF_W * 1.6 * 0.8),
        top_center=(x, LEG_SOLE_Y + 0.05, 0), top_size=(LEG_HALF_W * 1.2, LEG_HALF_W * 1.2 * 0.8),
        uv={k: _r("leg_lower") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    foot = box(
        center=(x, LEG_SOLE_Y, -0.08), size=(LEG_HALF_W * 1.4, 0.05, 0.22),
        uv={k: _r("foot") for k in ("front", "back", "left", "right", "top", "bottom")},
    )
    return merge(thigh, shin, foot)


def human_legs(phase: float = 0.0, closed: bool = True, stride: float = 1.0) -> Tuple[Mesh, Atlas]:
    """One mesh, same topology for every ``phase`` so walk/run keyframes
    stay morph-compatible (ASSET-CONTRACT 4.3). ``phase`` in [0,1) is the
    position in the gait cycle; left/right legs swing in opposite phase."""
    swing_deg = 22.0 * stride
    angle = math.radians(swing_deg) * math.sin(2 * math.pi * phase)
    left = _one_leg(-1)
    right = _one_leg(1)
    rotate_x_about(left, (-LEG_SPACING, LEG_HIP_Y, 0), angle)
    rotate_x_about(right, (LEG_SPACING, LEG_HIP_Y, 0), -angle)
    return merge(left, right), HUMAN_ATLAS


# -- Weapons ---------------------------------------------------------------
WEAPON_ATLAS_SIZE = 128


def _weapon_atlas() -> Atlas:
    a = Atlas(WEAPON_ATLAS_SIZE)
    a.pack([
        ("body", 64, 32), ("barrel", 48, 16), ("grip", 24, 32),
        ("stock", 32, 16), ("mag", 16, 32), ("misc", 32, 32),
    ])
    return a


def weapon(name: str = "", size: float = 1.0, grip=(0, 0, 0), muzzle=(0, 0, 0),
           kind: str = "pistol", source: str = "") -> Tuple[Mesh, Atlas]:
    atlas = _weapon_atlas()

    def rect(n):
        return atlas.uv(n)

    all_faces = ("front", "back", "left", "right", "top", "bottom")
    if kind == "grenade":
        bodym = cylinder(center=(0, 0, -0.10), radius=0.09, height=0.18, segments=8,
                          side_uv=rect("body"), cap_uv=rect("misc"))
        lever = box(center=(0.07, 0.07, -0.05), size=(0.015, 0.09, 0.03),
                    uv={k: rect("misc") for k in all_faces})
        mesh = merge(bodym, lever)
        return mesh, atlas

    # pistol / smg / rifle: grip block (around origin) + body + barrel
    # extending toward -Z (muzzle direction) + (rifle/smg) stock toward +Z.
    barrel_len = {"pistol": 0.55, "smg": 0.75, "rifle": 1.3}.get(kind, 0.6)
    body_len = {"pistol": 0.22, "smg": 0.32, "rifle": 0.40}.get(kind, 0.25)
    stock_len = {"pistol": 0.0, "smg": 0.18, "rifle": 0.32}.get(kind, 0.0)

    gripb = box(center=(0, -0.08, 0.04), size=(0.05, 0.16, 0.08),
                uv={k: rect("grip") for k in all_faces})
    body = box(center=(0, 0.02, -body_len / 2), size=(0.06, 0.08, body_len),
               uv={k: rect("body") for k in all_faces})
    barrel = cylinder(center=(0, -0.02, 0), radius=0.018,
                       height=barrel_len, segments=6, side_uv=rect("barrel"))
    # cylinder() builds its axis along Y; swap y/z so the barrel points
    # along -Z (muzzle direction) instead of standing upright.
    barrel.verts = [(x, 0.02 - z, -body_len - barrel_len / 2 + y) for x, y, z in barrel.verts]
    parts = [gripb, body, barrel]
    if stock_len:
        stock = tapered_box(
            base_center=(0, 0.02, body_len * 0.0), base_size=(0.05, 0.05),
            top_center=(0, -0.01, stock_len), top_size=(0.04, 0.04),
            uv={k: rect("stock") for k in all_faces},
        )
        parts.append(stock)
    if kind in ("rifle", "smg"):
        mag = tapered_box(
            base_center=(0, -0.06, -body_len * 0.4), base_size=(0.03, 0.05),
            top_center=(0, -0.22, -body_len * 0.4), top_size=(0.022, 0.03),
            uv={k: rect("mag") for k in all_faces},
        )
        parts.append(mag)
    mesh = merge(*parts)
    return mesh, atlas


# -- Bullet -----------------------------------------------------------------
def bullet() -> Tuple[Mesh, Atlas]:
    atlas = Atlas(64)
    atlas.pack([("skin", 64, 64)])
    uv = {k: atlas.uv("skin") for k in ("front", "back", "left", "right", "top", "bottom")}
    body = cylinder(center=(0, 0, 0.05), radius=0.05, height=0.25, segments=6,
                     side_uv=atlas.uv("skin"), cap_uv=atlas.uv("skin"))
    nose = box(center=(0, 0, -0.1), size=(0.08, 0.08, 0.08), uv=uv)
    return merge(body, nose), atlas


# -- Sky --------------------------------------------------------------------
def sky() -> Tuple[Mesh, Atlas]:
    """Box visible from inside, radius well under the ~50-unit unfogged
    limit derived in ASSET-CONTRACT 7."""
    atlas = Atlas(256)
    atlas.pack([
        ("front", 64, 64), ("back", 64, 64), ("left", 64, 64),
        ("right", 64, 64), ("top", 64, 64),
    ])
    half = 30.0
    m = box(center=(0, half * 0.3, 0), size=(half * 2, half * 2, half * 2), uv={
        "front": atlas.uv("front"), "back": atlas.uv("back"),
        "left": atlas.uv("left"), "right": atlas.uv("right"), "top": atlas.uv("top"),
    })
    flip_mesh(m)
    return m, atlas


GENERATORS = {
    "human_upper": human_upper,
    "human_arms": human_arms,
    "human_legs": human_legs,
    "weapon": weapon,
    "bullet": bullet,
    "sky": sky,
}

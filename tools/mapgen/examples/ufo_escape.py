#!/usr/bin/env python3
"""Programmatic authoring for UFO ESCAPE (docs/MISSION-UFO-MAP-ESCAPE.md §5).

Builds tools/mapgen/examples/ufo_escape.json: a winding, two-deck alien-ship
escape route (cell block -> maintenance crawlway -> specimen lab -> goo
processing hall -> switchback ramp -> bridge -> escape-pod bay), deliberately
NOT an arena: short sight lines, doglegs, staggered cover, two floor levels.

Run: python3 tools/mapgen/examples/ufo_escape.py
     (writes tools/mapgen/examples/ufo_escape.json next to this file)
"""
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

HERE = Path(__file__).resolve().parent

TEXTURES = [
    "hull.bmp", "floor.bmp", "ceiling.bmp", "goo.bmp", "vat.bmp",
    "labpanel.bmp", "lightstrip.bmp", "hazard.bmp", "console.bmp", "bulkhead.bmp",
]

WALL_T = 4.0      # wall thickness
SLAB_T = 2.0      # floor/ceiling slab thickness

BLOCKS: List[Dict] = []
POINTS: List[Dict] = []


def _box(comment, vmin, vmax, texture, units=32.0, faces=None) -> None:
    entry = {"comment": comment, "min": list(vmin), "max": list(vmax),
              "texture_world_units": units}
    if faces:
        entry["faces"] = faces
    else:
        entry["texture"] = texture
    BLOCKS.append(entry)


def floor_slab(comment, x0, x1, z0, z1, y, texture="floor.bmp", units=32.0):
    _box(comment, (x0, y - SLAB_T, z0), (x1, y, z1), texture, units)


def ceiling_slab(comment, x0, x1, z0, z1, y, texture="ceiling.bmp", units=32.0):
    _box(comment, (x0, y, z0), (x1, y + SLAB_T, z1), texture, units)


def _gaps(lo, hi, gap: Optional[Tuple[float, float]]):
    if gap is None:
        return [(lo, hi)]
    ga, gb = gap
    ga, gb = max(lo, ga), min(hi, gb)
    out = []
    if ga - lo > 0.5:
        out.append((lo, ga))
    if hi - gb > 0.5:
        out.append((gb, hi))
    return out


def wall_x(comment, x, z0, z1, y0, y1, texture="hull.bmp", gap=None, units=32.0):
    """Wall whose face lies at fixed x, spanning z in [z0, z1]."""
    for i, (a, b) in enumerate(_gaps(min(z0, z1), max(z0, z1), gap)):
        _box(f"{comment} [{i}]", (x - WALL_T / 2, y0, a), (x + WALL_T / 2, y1, b), texture, units)


def wall_z(comment, z, x0, x1, y0, y1, texture="hull.bmp", gap=None, units=32.0):
    """Wall whose face lies at fixed z, spanning x in [x0, x1]."""
    for i, (a, b) in enumerate(_gaps(min(x0, x1), max(x0, x1), gap)):
        _box(f"{comment} [{i}]", (a, y0, z - WALL_T / 2), (b, y1, z + WALL_T / 2), texture, units)


def box_obstacle(comment, x0, x1, y0, y1, z0, z1, texture, units=16.0):
    _box(comment, (x0, y0, z0), (x1, y1, z1), texture, units)


def ramp_block(comment, x0, x1, z0, z1, y0, y1, texture="floor.bmp", edge_texture="hazard.bmp"):
    """Raw 8-vertex sloped block: floor rises from y0 (at x0) to y1 (at x1),
    ceiling-less (just the walking surface + a thin slab underneath it)."""
    thick = 4.0
    verts = [
        [x0, y0, z0], [x1, y1, z0], [x1, y1, z1], [x0, y0, z1],
        [x0, y0 - thick, z0], [x1, y1 - thick, z0], [x1, y1 - thick, z1], [x0, y0 - thick, z1],
    ]
    tex = {"texture": texture, "u": [0.0, 0.0, 1.0, 1.0], "v": [0.0, 0.0, 1.0, 1.0]}
    etex = {"texture": edge_texture, "u": [0.0, 0.0, 1.0, 1.0], "v": [0.0, 0.0, 1.0, 1.0]}
    faces = [dict(tex), dict(tex), dict(etex), dict(etex), dict(etex), dict(etex)]
    BLOCKS.append({"comment": comment, "vertices": verts, "faces": faces})


def octagonal_vat(comment, cx, cz, half, y0, y1, side_tex="vat.bmp", top_tex="goo.bmp"):
    """One octagonal vat from 3 raw blocks: a centre box + two trapezoid
    end-caps, side faces vat.bmp, top goo.bmp."""
    chamfer = half * 0.45
    tex_top = {"texture": top_tex, "u": [0.0, 0.0, 1.0, 1.0], "v": [0.0, 0.0, 1.0, 1.0]}
    tex_side = {"texture": side_tex, "u": [0.0, 0.0, 1.0, 1.0], "v": [0.0, 0.0, 1.0, 1.0]}

    def vat_box(tag, x0, x1, z0, z1):
        verts = [
            [x0, y1, z0], [x1, y1, z0], [x1, y1, z1], [x0, y1, z1],
            [x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1],
        ]
        faces = [dict(tex_top), dict(tex_side), dict(tex_side), dict(tex_side),
                 dict(tex_side), dict(tex_side)]
        BLOCKS.append({"comment": f"{comment} {tag}", "vertices": verts, "faces": faces})

    vat_box("centre", cx - half, cx + half, cz - half + chamfer, cz + half - chamfer)
    vat_box("north cap", cx - half + chamfer, cx + half - chamfer, cz - half, cz - half + chamfer)
    vat_box("south cap", cx - half + chamfer, cx + half - chamfer, cz + half - chamfer, cz + half)


def point(kind, pos, rotation=0.0, **kw):
    entry = {"type": kind, "pos": list(pos), "rotation": rotation}
    entry.update(kw)
    POINTS.append(entry)
    return entry


# ---------------------------------------------------------------------------
# 1. Cell block (lower deck, south-west). Spawn in the southern cell.
# ---------------------------------------------------------------------------
# Corridor runs north-south at x in [-12, 12]; three cells open onto its
# west side (x < -12) from south to north: spawn cell, tube cell, empty cell.
Y0, Y1 = 0.0, 40.0
CB_Z0, CB_Z1 = 256.0, 404.0

floor_slab("cellblock floor", -72.0, 24.0, CB_Z0, CB_Z1, Y0)
ceiling_slab("cellblock ceiling", -72.0, 24.0, CB_Z0, CB_Z1, Y1)

wall_z("cellblock south wall", CB_Z1, -72.0, 24.0, Y0, Y1)
wall_x("cellblock east wall", 24.0, CB_Z0, CB_Z1, Y0, Y1)
# West outer wall of the cell row, with no gap (cells are reached via the
# corridor, not from outside).
wall_x("cellblock west wall", -72.0, CB_Z0, CB_Z1, Y0, Y1)

# Internal partitions separating corridor (x>-12) from the 3 cells, each with
# a doorway gap facing east into the corridor.
cell_z_bounds = [(360.0, 404.0), (312.0, 352.0), (264.0, 304.0)]  # south->north
for i, (za, zb) in enumerate(cell_z_bounds):
    wall_x(f"cell {i} corridor wall", -12.0, za, zb, Y0, Y1, gap=((za + zb) / 2 - 11, (za + zb) / 2 + 11))
    # cell end caps (between cells)
for za, zb in [(304.0, 312.0), (352.0, 360.0)]:
    wall_z(f"cell divider", zb, -72.0, -12.0, Y0, Y1)

wall_z("cellblock north wall of cell row", CB_Z0, -72.0, -12.0, Y0, Y1)

# North end of the corridor opens into the maintenance crawlway dogleg.
wall_z("cellblock north corridor wall", CB_Z0, -12.0, 24.0, Y0, Y1, gap=(-10.0, 10.0))

PLAYER_SPAWN = (-42.0, 2.0, 390.0)
# Engine facing: rotation r looks along (sin r, cos r) in x/z (gamemain.cpp
# first-person camera crx = pi/2 - r), so face(dx, dz) = atan2(dx, dz).
def face(dx, dz):
    return math.atan2(dx, dz)

SPAWN_DOOR = (-12.0, 382.0)  # gap centre of the spawn cell's corridor wall
point("player_spawn", PLAYER_SPAWN, rotation=face(SPAWN_DOOR[0] - PLAYER_SPAWN[0], SPAWN_DOOR[1] - PLAYER_SPAWN[2]), human_type=47)

# Specimen tube prop in cell 1 (the middle cell).
point("small_object", (-42.0, 2.0, 332.0), rotation=0.0, object_type=12)

# Guard 1: drone, ~30 units north of the spawn doorway (door at z=360),
# back turned (facing north, rotation 0, same convention as the spawn).
GUARD1_WAYPT_ID = 20
# ~33 units from the doorway, looking north (rotation pi) up the corridor,
# so his back is to the cell. The wait waypoint's rotation is what the AI
# keeps turning to, so it must face north too.
GUARD1_POS = (2.0, 2.0, 352.0)
point("waypoint", GUARD1_POS, rotation=face(0.0, -1.0), mode=2, p3=GUARD1_WAYPT_ID, p4=GUARD1_WAYPT_ID)
point("human", GUARD1_POS, rotation=face(0.0, -1.0), human_type=44, team=2, ai_path=GUARD1_WAYPT_ID)

# ---------------------------------------------------------------------------
# 2. Maintenance crawlway: two 90-degree doglegs between cell block and lab.
# ---------------------------------------------------------------------------
floor_slab("crawl seg A floor", -12.0, 24.0, 176.0, 256.0, Y0)
ceiling_slab("crawl seg A ceiling", -12.0, 24.0, 176.0, 256.0, Y1)
wall_x("crawl seg A west wall", -12.0, 176.0, 256.0, Y0, Y1)
wall_x("crawl seg A east wall", 24.0, 176.0, 256.0, Y0, Y1, gap=(188.0, 208.0))
# Seal the north end of segment A for x < 24: the only connection onward is
# the east-wall gap above into segment B; without this wall the corner
# between A's north edge (z=176) and B's start (x=24) is open to the void.
wall_z("crawl seg A north wall", 176.0, -12.0, 24.0, Y0, Y1)

# Dogleg 1: turn east.
floor_slab("crawl seg B floor", 24.0, 96.0, 180.0, 208.0, Y0)
ceiling_slab("crawl seg B ceiling", 24.0, 96.0, 180.0, 208.0, Y1)
wall_z("crawl seg B south wall", 208.0, 24.0, 96.0, Y0, Y1)
wall_z("crawl seg B north wall", 180.0, 24.0, 96.0, Y0, Y1, gap=(72.0, 92.0))
wall_x("crawl seg B east wall", 96.0, 180.0, 208.0, Y0, Y1)

# Dogleg 2: turn north toward the lab's south entrance.
floor_slab("crawl seg C floor", 68.0, 96.0, 80.0, 180.0, Y0)
ceiling_slab("crawl seg C ceiling", 68.0, 96.0, 80.0, 180.0, Y1)
wall_x("crawl seg C west wall", 68.0, 80.0, 180.0, Y0, Y1)
wall_x("crawl seg C east wall", 96.0, 80.0, 180.0, Y0, Y1)

# Low non-blocking pipe trim on the east wall of seg A (decorative; thin
# enough to never obstruct a 2.8-radius player).
box_obstacle("crawl pipe trim", 20.0, 22.0, 4.0, 10.0, 180.0, 254.0, "bulkhead.bmp")

CRAWL_PATROL = 21
point("waypoint", (0.0, 2.0, 230.0), mode=0, p3=CRAWL_PATROL + 1, p4=CRAWL_PATROL)
point("waypoint", (60.0, 2.0, 194.0), mode=0, p3=CRAWL_PATROL + 2, p4=CRAWL_PATROL + 1)
point("waypoint", (82.0, 2.0, 110.0), mode=0, p3=CRAWL_PATROL, p4=CRAWL_PATROL + 2)
point("human", (0.0, 2.0, 230.0), rotation=0.0, human_type=44, team=2, ai_path=CRAWL_PATROL)

# ---------------------------------------------------------------------------
# 3. Specimen lab (lower deck, large room, staggered vats, catwalk).
# ---------------------------------------------------------------------------
LAB_X0, LAB_X1 = 0.0, 180.0
LAB_Z0, LAB_Z1 = -60.0, 80.0

floor_slab("lab floor", LAB_X0, LAB_X1, LAB_Z0, LAB_Z1, Y0)
ceiling_slab("lab ceiling", LAB_X0, LAB_X1, LAB_Z0, LAB_Z1, Y1)
wall_z("lab south wall", LAB_Z1, LAB_X0, LAB_X1, Y0, Y1, gap=(68.0, 96.0), texture="labpanel.bmp")
wall_z("lab north wall (far door)", LAB_Z0, LAB_X0, LAB_X1, Y0, Y1, texture="labpanel.bmp")
wall_x("lab west wall", LAB_X0, LAB_Z0, LAB_Z1, Y0, Y1, texture="labpanel.bmp")
wall_x("lab east wall", LAB_X1, LAB_Z0, LAB_Z1, Y0, Y1)

# 3 staggered octagonal goo vats: no aisle runs straight through.
octagonal_vat("vat A", 50.0, 0.0, 16.0, Y0, Y0 + 26.0)
octagonal_vat("vat B", 100.0, -30.0, 16.0, Y0, Y0 + 26.0)
octagonal_vat("vat C", 140.0, 20.0, 16.0, Y0, Y0 + 26.0)

# Catwalk around two walls at y 24 with a short access ramp.
floor_slab("lab catwalk west", LAB_X0 + 2.0, LAB_X0 + 18.0, LAB_Z0 + 2.0, LAB_Z1 - 2.0, Y0 + 24.0,
            texture="floor.bmp", units=16.0)
floor_slab("lab catwalk north", LAB_X0 + 2.0, LAB_X1 - 2.0, LAB_Z0 + 2.0, LAB_Z0 + 18.0, Y0 + 24.0,
            texture="floor.bmp", units=16.0)
ramp_block("lab catwalk access ramp", LAB_X0 + 18.0, LAB_X0 + 38.0, 60.0, 76.0, Y0 + 2.0, Y0 + 24.0)

# Entrance faces vat A within 30 units: cover against the south doorway.
point("event", (82.0, 2.0, 70.0), event_id=13, p2=0, p3=-999, p4=-999)  # placeholder id, fixed below

# Staged group 1: 2 drones at the lab far (north) door, triggered on first entry.
point("staged_human", (145.0, 2.0, -50.0), rotation=3.14159, human_type=44, team=2, ai_path=0, wave=1)
point("staged_human", (155.0, 2.0, -50.0), rotation=3.14159, human_type=44, team=2, ai_path=0, wave=1)

# Weapon pickup: PHOTON SMG (25) in the lab.
point("weapon_pickup", (60.0, 2.0, 35.0), weapon_id=25, ammo=120)

# Specimen tubes along the lab walls / cell 2.
for (tx, tz) in [(8.0, -40.0), (8.0, 0.0), (8.0, 40.0), (172.0, -40.0), (172.0, 0.0), (172.0, 40.0)]:
    point("small_object", (tx, 2.0, tz), object_type=12)

LAB_PATROL_IDS = [30, 31, 32, 33, 34, 35]
lab_patrol_pts = [(30.0, 2.0, -30.0), (80.0, 2.0, 50.0), (120.0, 2.0, -10.0),
                   (160.0, 2.0, 60.0), (40.0, 2.0, 55.0), (130.0, 2.0, -45.0)]
for i, (pid, pos) in enumerate(zip(LAB_PATROL_IDS, lab_patrol_pts)):
    nxt = LAB_PATROL_IDS[(i + 1) % len(LAB_PATROL_IDS)]
    point("waypoint", pos, mode=0, p3=nxt, p4=pid)
for pid, pos in list(zip(LAB_PATROL_IDS, lab_patrol_pts))[:4]:
    point("human", pos, rotation=0.0, human_type=44 if pid % 2 == 0 else 45, team=2, ai_path=pid)

# ---------------------------------------------------------------------------
# 4. Goo processing hall (lower deck, long; staggered cover).
# ---------------------------------------------------------------------------
HALL_X0, HALL_X1 = 180.0, 380.0
HALL_Z0, HALL_Z1 = -50.0, 10.0

floor_slab("hall floor", HALL_X0, HALL_X1, HALL_Z0, HALL_Z1, Y0)
ceiling_slab("hall ceiling", HALL_X0, HALL_X1, HALL_Z0, HALL_Z1, Y1)
wall_z("hall south wall", HALL_Z1, HALL_X0, HALL_X1, Y0, Y1)
wall_z("hall north wall", HALL_Z0, HALL_X0, HALL_X1, Y0, Y1)
wall_x("hall west wall (from lab)", HALL_X0, HALL_Z0, HALL_Z1, Y0, Y1, gap=(-30.0, 0.0))
wall_x("hall east wall (to ramp)", HALL_X1, HALL_Z0, HALL_Z1, Y0, Y1, gap=(-33.0, -12.0))

# Serpentine baffle walls: each spans from one long wall partway across
# the hall (full height Y0-Y1), alternating sides, so the path zig-zags
# and no straight line along the hall's long axis stays clear for long.
baffles = [
    (210.0, 220.0, "south", -20.0),  # attaches to south wall (z=10), reaches z=-20
    (250.0, 260.0, "north", -20.0),  # attaches to north wall (z=-50), reaches z=-20
    (290.0, 300.0, "south", -20.0),
    (330.0, 340.0, "north", -20.0),
]
for i, (x0, x1, side, reach) in enumerate(baffles):
    if side == "south":
        box_obstacle(f"hall baffle {i}", x0, x1, Y0, Y1, reach, HALL_Z1, "bulkhead.bmp")
    else:
        box_obstacle(f"hall baffle {i}", x0, x1, Y0, Y1, HALL_Z0, reach, "bulkhead.bmp")
# Low goo troughs (12 tall, non-blocking at eye height) for flavour/cover.
trough_positions = [(240.0, -20.0), (280.0, -20.0), (320.0, -20.0)]
for i, (tx, tz) in enumerate(trough_positions):
    box_obstacle(f"hall trough {i}", tx - 10.0, tx + 10.0, Y0, Y0 + 12.0, tz - 6.0, tz + 6.0, "goo.bmp", units=16.0)

HALL_PATROL_IDS = [40, 41, 42, 43]
hall_patrol_pts = [(210.0, 2.0, -20.0), (270.0, 2.0, -42.0), (330.0, 2.0, -20.0), (270.0, 2.0, 2.0)]
for i, (pid, pos) in enumerate(zip(HALL_PATROL_IDS, hall_patrol_pts)):
    nxt = HALL_PATROL_IDS[(i + 1) % len(HALL_PATROL_IDS)]
    point("waypoint", pos, mode=0, p3=nxt, p4=pid)
for pid, pos in list(zip(HALL_PATROL_IDS, hall_patrol_pts))[:3]:
    point("human", pos, rotation=0.0, human_type=45, team=2, ai_path=pid)

# ---------------------------------------------------------------------------
# 5. Switchback ramp: y 0 -> y 44 across two ramps + a landing.
# ---------------------------------------------------------------------------
RAMP_X0, RAMP_X1 = 380.0, 428.0
RAMP_SHAFT_X1 = RAMP_X1 + 48.0  # 476.0: east end of the whole switchback shaft
# The switchback ramp is a single two-deck-tall shaft (y 0..88, matching the
# hull/bridge ceiling) rather than three separately-capped segments: capping
# each segment individually left thin uncapped slivers wherever the ramp's
# footprint (z -32..-13) didn't exactly match an approach/landing footprint
# (z -35..-10). One full-height perimeter + one ceiling at the top avoids
# that class of seam gap entirely.
RAMP_SHAFT_X0 = RAMP_X0 - 4.0  # extra ceiling margin kept for hall overlap
wall_z("ramp shaft south wall", -35.0, RAMP_X0, RAMP_SHAFT_X1, Y0, 88.0)
wall_z("ramp shaft north wall", -10.0, RAMP_X0, RAMP_SHAFT_X1, Y0, 88.0)
ceiling_slab("ramp shaft ceiling", RAMP_X0, RAMP_SHAFT_X1, -35.0, -10.0, 88.0)
# Attic filler: closes the vertical gap above the hall's own low ceiling
# (y40) right at the seam where the hall's roof height (40) meets the
# switchback shaft's roof height (88); without it a ray at shaft height can
# graze west over the hall's ceiling top and escape through unenclosed
# airspace above the hall before the shaft's own walls would catch it.
_box("ramp/hall seam attic filler", (HALL_X1 - 40.0, Y1, -35.0), (RAMP_X0, 88.0, -10.0), "hull.bmp")

ramp_block("switchback ramp A", RAMP_X0, RAMP_X1, -32.0, -13.0, Y0 + 2.0, 22.0)
wall_x("ramp A low rail", RAMP_X0, -32.0, -13.0, 0.0, 4.0, "hazard.bmp", units=8.0)
floor_slab("ramp landing floor", RAMP_X1, RAMP_X1 + 20.0, -35.0, -10.0, 22.0)
ramp_block("switchback ramp B", RAMP_X1 + 20.0, RAMP_X1 + 48.0, -32.0, -13.0, 22.0 + 2.0, 44.0)
wall_x("ramp B low rail", RAMP_X1 + 48.0, -32.0, -13.0, 22.0, 26.0, "hazard.bmp", units=8.0)

# ---------------------------------------------------------------------------
# 6. Bridge (upper deck): ring of consoles, central pillar, dais.
# ---------------------------------------------------------------------------
UY0, UY1 = 44.0, 88.0
BR_X0, BR_X1 = 448.0, 560.0
BR_Z0, BR_Z1 = -70.0, 40.0

floor_slab("bridge floor", BR_X0, BR_X1, BR_Z0, BR_Z1, UY0)
ceiling_slab("bridge ceiling", BR_X0, BR_X1, BR_Z0, BR_Z1, UY1)
wall_z("bridge south wall", BR_Z1, BR_X0, BR_X1, UY0, UY1)
wall_z("bridge north wall", BR_Z0, BR_X0, BR_X1, UY0, UY1, gap=(495.0, 525.0))
wall_x("bridge west wall", BR_X0, BR_Z0, BR_Z1, UY0, UY1, texture="lightstrip.bmp")
wall_x("bridge east wall", BR_X1, BR_Z0, BR_Z1, UY0, UY1, texture="lightstrip.bmp")

box_obstacle("bridge holo pillar", 498.0, 506.0, UY0, UY0 + 30.0, -19.0, -11.0, "console.bmp")
console_positions = [(475.0, -35.0), (530.0, -35.0), (475.0, 10.0), (530.0, 10.0)]
for i, (cx, cz) in enumerate(console_positions):
    box_obstacle(f"bridge console {i}", cx - 6.0, cx + 6.0, UY0, UY0 + 6.0, cz - 4.0, cz + 4.0, "console.bmp")
floor_slab("captains dais", 490.0, 516.0, 18.0, 34.0, UY0 + 4.0, "console.bmp", units=16.0)

point("weapon_pickup", (502.0, UY0 + 6.0, -36.0), weapon_id=26, ammo=80)

BRIDGE_PATROL_IDS = [50, 51, 52]
bridge_patrol_pts = [(470.0, UY0 + 2.0, -50.0), (540.0, UY0 + 2.0, -50.0), (502.0, UY0 + 2.0, 20.0)]
for i, (pid, pos) in enumerate(zip(BRIDGE_PATROL_IDS, bridge_patrol_pts)):
    nxt = BRIDGE_PATROL_IDS[(i + 1) % len(BRIDGE_PATROL_IDS)]
    point("waypoint", pos, mode=0, p3=nxt, p4=pid)
for pid, pos in list(zip(BRIDGE_PATROL_IDS, bridge_patrol_pts))[:2]:
    point("human", pos, rotation=0.0, human_type=46, team=2, ai_path=pid)

# ---------------------------------------------------------------------------
# 7. Escape-pod bay (upper deck): final ambush, winning pod.
# ---------------------------------------------------------------------------
POD_X0, POD_X1 = 448.0, 560.0
POD_Z0, POD_Z1 = -190.0, -70.0

floor_slab("pod bay floor", POD_X0, POD_X1, POD_Z0, POD_Z1, UY0)
ceiling_slab("pod bay ceiling", POD_X0, POD_X1, POD_Z0, POD_Z1, UY1)
wall_z("pod bay south wall (from bridge)", POD_Z1, POD_X0, POD_X1, UY0, UY1, gap=(495.0, 525.0))
wall_z("pod bay north wall", POD_Z0, POD_X0, POD_X1, UY0, UY1, texture="bulkhead.bmp")
wall_x("pod bay west wall", POD_X0, POD_Z0, POD_Z1, UY0, UY1, texture="bulkhead.bmp")
wall_x("pod bay east wall", POD_X1, POD_Z0, POD_Z1, UY0, UY1, texture="bulkhead.bmp")

pod_positions = [(470.0, -184.0), (502.0, -184.0), (534.0, -184.0)]
for i, (px, pz) in enumerate(pod_positions):
    box_obstacle(f"escape pod {i}", px - 8.0, px + 8.0, UY0, UY0 + 20.0, pz - 10.0, pz + 10.0, "bulkhead.bmp", units=16.0)
ESCAPE_POD = (502.0, UY0 + 2.0, -174.0)

# Staged group 2: 2 brutes from side hatches, triggered on bay entry.
point("staged_human", (449.0, UY0 + 2.0, -121.0), rotation=1.5708, human_type=46, team=2, ai_path=0, wave=2)
point("staged_human", (559.0, UY0 + 2.0, -121.0), rotation=-1.5708, human_type=46, team=2, ai_path=0, wave=2)

# A couple of brutes already standing watch near the pods (off-axis).
point("waypoint", (470.0, UY0 + 2.0, -150.0), mode=2, p3=60, p4=60)
point("human", (470.0, UY0 + 2.0, -150.0), rotation=3.14159, human_type=46, team=2, ai_path=60)

# ---------------------------------------------------------------------------
# Events on line -100: wake up -> headshot the guard -> lab arrival ->
# alarm -> deploy group 1 -> pod-bay arrival -> message -> deploy group 2 ->
# pod arrival -> complete. See src/event.cpp for p1 opcodes.
# ---------------------------------------------------------------------------
LAB_ENTRY = (82.0, 2.0, 70.0)
POD_BAY_ENTRY = (502.0, UY0 + 2.0, -75.0)

# Drop the placeholder lab-cover event point authored above; it was only a
# visual reminder of "entrance faces a vat" and is not part of the chain.
POINTS[:] = [p for p in POINTS if not (p.get("type") == "event" and p.get("p2") == 0 and p.get("p3") == -999)]

EVT_WAIT2 = -100
EVT_WAKE_MSG = -1
EVT_LAB_ARRIVAL = -2
EVT_ALARM_MSG = -3
EVT_DEPLOY1 = -4
EVT_PODBAY_ARRIVAL = -5
EVT_AMBUSH_MSG = -6
EVT_DEPLOY2 = -7
EVT_POD_ARRIVAL = -8
EVT_COMPLETE = -9

point("event", (0.0, 0.0, 0.0), event_id=17, p2=2, p3=EVT_WAKE_MSG, p4=EVT_WAIT2)       # wait 2s
point("event", (0.0, 0.0, 0.0), event_id=18, p2=0, p3=EVT_LAB_ARRIVAL, p4=EVT_WAKE_MSG)  # msg 0: WAKE UP
point("event", LAB_ENTRY, event_id=13, p2=0, p3=EVT_ALARM_MSG, p4=EVT_LAB_ARRIVAL)       # arrival wait, lab door
point("event", (0.0, 0.0, 0.0), event_id=18, p2=1, p3=EVT_DEPLOY1, p4=EVT_ALARM_MSG)     # msg 1: ALARM
# event_id 41 (deploy wave) is outside the author "event" kind's [10,19]
# range (that range is the p1 opcode space for the general event engine;
# 40/41 are the separate wave-deploy opcodes, src/event.cpp), so these two
# chain links are authored as "raw" points with p1=41 directly.
point("raw", (0.0, 0.0, 0.0), p1=41, p2=1, p3=EVT_PODBAY_ARRIVAL, p4=EVT_DEPLOY1)  # deploy group 1
point("event", POD_BAY_ENTRY, event_id=13, p2=0, p3=EVT_AMBUSH_MSG, p4=EVT_PODBAY_ARRIVAL)  # arrival wait, pod bay door
point("event", (0.0, 0.0, 0.0), event_id=18, p2=2, p3=EVT_DEPLOY2, p4=EVT_AMBUSH_MSG)    # msg 2: AMBUSH
point("raw", (0.0, 0.0, 0.0), p1=41, p2=2, p3=EVT_POD_ARRIVAL, p4=EVT_DEPLOY2)   # deploy group 2
point("event", ESCAPE_POD, event_id=13, p2=0, p3=EVT_COMPLETE, p4=EVT_POD_ARRIVAL)       # arrival wait, the pod
point("event", (0.0, 0.0, 0.0), event_id=10, p2=0, p3=0, p4=EVT_COMPLETE)                # complete


def build_doc() -> Dict:
    return {"schema": 1, "textures": TEXTURES, "blocks": BLOCKS, "points": POINTS}


def main() -> int:
    doc = build_doc()
    out_path = HERE / "ufo_escape.json"
    out_path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out_path}: {len(BLOCKS)} blocks, {len(POINTS)} points")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

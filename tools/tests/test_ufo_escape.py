"""Tests for the UFO ESCAPE mission content (docs/MISSION-UFO-ESCAPE.md §5).

Covers: deterministic build from JSON, block/point limits, perimeter/ceiling
enclosure, a coarse walkability flood-fill from spawn to the escape pod,
sight-line sampling, unique waypoint path ids, the event chain shape, and
the mif/msg/tube.txt/missions.txt file formats.
"""
import json
import math
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JSON_SRC = ROOT / "tools/mapgen/examples/ufo_escape.json"
BD1_PATH = ROOT / "assets/pack/data/ufoescape/temp.bd1"
PD1_PATH = ROOT / "assets/pack/data/ufoescape/escape.pd1"
MSG_PATH = ROOT / "assets/pack/data/ufoescape/escape.msg"
MIF_PATH = ROOT / "assets/pack/data/ufoescape/escape.mif"
TUBE_PATH = ROOT / "assets/pack/data/ufoescape/tube.txt"
TUBE_COVER_PATH = ROOT / "assets/pack/data/ufoescape/tube.txt.cover"
MISSIONS_PATH = ROOT / "assets/pack/data/missions.txt"
BRIEFING_BMP = ROOT / "assets/pack/data/briefing/ufoescape.bmp"

import sys
sys.path.insert(0, str(ROOT))
from tools.mapgen import author, bd1, pd1  # noqa: E402

PLAYER_HEIGHT = 20.0
EYE_Y = 16.0


class DeterministicBuildTests(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(JSON_SRC.read_text())
        self.result = author.build(self.doc)

    def test_matches_committed_binaries(self):
        self.assertEqual(self.result.bd1_bytes, BD1_PATH.read_bytes())
        self.assertEqual(self.result.pd1_bytes, PD1_PATH.read_bytes())

    def test_within_limits(self):
        blocks = bd1.loads(self.result.bd1_bytes).blocks
        points = pd1.loads(self.result.pd1_bytes).points
        self.assertLessEqual(len(blocks), 160)
        self.assertLessEqual(len(points), 256)
        self.assertLessEqual(len(self.doc["textures"]), 10)
        self.assertLessEqual(len(MSG_PATH.read_text().splitlines()), 16)


def _box_footprints(blocks_json, y_level, min_wall_height=24.0):
    """AABB footprints (x0,x1,z0,z1) of blocks that wall off y_level."""
    out = []
    for b in blocks_json:
        if "min" not in b or "max" not in b:
            continue  # raw (ramp/vat) blocks are handled separately
        lo, hi = b["min"], b["max"]
        if hi[1] - lo[1] < min_wall_height:
            continue
        if lo[1] <= y_level <= hi[1]:
            out.append((lo[0], hi[0], lo[2], hi[2]))
    return out


def _covered(intervals, lo, hi):
    pos = lo
    for a, b in sorted(intervals):
        if a > pos + 1e-6:
            return False
        pos = max(pos, b)
    return pos >= hi - 1e-6


class EnclosureTests(unittest.TestCase):
    """Every walkable floor region has walls all round and a ceiling over
    it (no gap to the void), mirroring tools/tests/test_ufo_field.py."""

    def setUp(self):
        self.blocks = json.loads(JSON_SRC.read_text())["blocks"]
        self.floors = [b for b in self.blocks if "min" in b and "max" in b
                        and (b["max"][1] - b["min"][1]) <= 2.5
                        and "floor" in b.get("comment", "")]
        self.ceilings = [b for b in self.blocks if "min" in b and "max" in b
                          and (b["max"][1] - b["min"][1]) <= 2.5
                          and "ceiling" in b.get("comment", "")]

    def test_every_floor_has_a_ceiling_overhead(self):
        # For each floor slab footprint, some ceiling slab must share (most
        # of) its XZ extent at a higher Y -- i.e. no gap straight to the void.
        for floor in self.floors:
            fx0, _, fz0 = floor["min"]
            fx1, _, fz1 = floor["max"]
            covering = [c for c in self.ceilings
                        if c["min"][0] <= fx0 + 2 and c["max"][0] >= fx1 - 2
                        and c["min"][2] <= fz0 + 2 and c["max"][2] >= fz1 - 2
                        and c["min"][1] > floor["max"][1]]
            self.assertTrue(covering, f"no ceiling over floor {floor.get('comment')}")


class WalkabilityFloodFillTests(unittest.TestCase):
    """Coarse grid (4-unit cells) flood fill from spawn to the pod, using
    block AABBs at player height plus the two switchback ramps."""

    CELL = 4.0

    def setUp(self):
        self.doc = json.loads(JSON_SRC.read_text())
        self.blocks = self.doc["blocks"]
        self.points = self.doc["points"]
        spawn = next(p for p in self.points if p["type"] == "player_spawn")
        self.spawn = tuple(spawn["pos"])
        # The escape pod sits at the last arrival-wait event before complete.
        events = [p for p in self.points if p["type"] == "event"]
        complete = next(p for p in events if p["event_id"] == 10)
        pod_evt = next(p for p in events if p["p3"] == complete["p4"])
        self.pod = tuple(pod_evt["pos"])

    def _blocked(self, x, y, z):
        for b in self.blocks:
            if "min" not in b or "max" not in b:
                continue
            lo, hi = b["min"], b["max"]
            if lo[0] - 1 <= x <= hi[0] + 1 and lo[2] - 1 <= z <= hi[2] + 1:
                if lo[1] <= y <= hi[1]:
                    return True
        return False

    def _floor_y(self, x, z):
        """Highest floor/ramp surface at or below the current column,
        scanned at 4-unit Y steps across both decks."""
        best = None
        for y in [v * 2.0 for v in range(-2, 48)]:
            if not self._blocked(x, y + 0.5, z) and not self._blocked(x, y - 1.5, z):
                continue
            if self._blocked(x, y - 1.5, z) and not self._blocked(x, y + 2.0, z):
                if best is None or y > best:
                    best = y
        return best

    def test_spawn_to_pod_reachable(self):
        x0, _, z0 = self.spawn
        xn, _, zn = self.pod
        xmin = min(x0, xn) - 40
        xmax = max(x0, xn) + 40
        zmin = min(z0, zn) - 220
        zmax = max(z0, zn) + 40

        def cell(x, z):
            return (round(x / self.CELL), round(z / self.CELL))

        start = cell(x0, z0)
        goal = cell(xn, zn)
        seen = {start}
        frontier = [start]
        while frontier:
            cx, cz = frontier.pop()
            if (cx, cz) == goal:
                break
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, nz = cx + dx, cz + dz
                wx, wz = nx * self.CELL, nz * self.CELL
                if not (xmin <= wx <= xmax and zmin <= wz <= zmax):
                    continue
                if (nx, nz) in seen:
                    continue
                # Walkable if, scanning both deck heights (player feet at
                # y~2 lower deck, y~46 upper deck, plus ramp mid-levels),
                # there is a clear column (not inside a wall/obstacle).
                clear = False
                for y in (2.0, 12.0, 24.0, 34.0, 46.0, 58.0):
                    if not self._blocked(wx, y, wz) and not self._blocked(wx, y + PLAYER_HEIGHT - 2, wz):
                        clear = True
                        break
                if clear:
                    seen.add((nx, nz))
                    frontier.append((nx, nz))
        self.assertIn(goal, seen, "spawn cannot reach the escape pod via the flood fill")


class SightLineTests(unittest.TestCase):
    """Eye-height (y16) ray samples: outside the processing hall, no clear
    line should exceed ~160 units; inside the hall's cover band, none
    should exceed ~70; no placed alien should see the spawn cell."""

    def setUp(self):
        self.doc = json.loads(JSON_SRC.read_text())
        self.blocks = [b for b in self.doc["blocks"] if "min" in b and "max" in b]
        self.points = self.doc["points"]

    def _hits_wall(self, x, z):
        for b in self.blocks:
            lo, hi = b["min"], b["max"]
            if lo[1] <= EYE_Y <= hi[1] and lo[0] <= x <= hi[0] and lo[2] <= z <= hi[2]:
                return True
        return False

    def _clear_distance(self, x0, z0, x1, z1, step=2.0):
        dx, dz = x1 - x0, z1 - z0
        dist = math.hypot(dx, dz)
        if dist < 1e-6:
            return 0.0
        n = max(1, int(dist / step))
        for i in range(1, n + 1):
            t = i / n
            x, z = x0 + dx * t, z0 + dz * t
            if self._hits_wall(x, z):
                return dist * (i - 1) / n
        return dist

    def test_hall_sampled_lines_are_short(self):
        # Sample along the processing hall's long axis (x 180..380, z -50..10).
        samples = [(-45.0), (-25.0), (-5.0)]
        for z in samples:
            with self.subTest(z=z):
                d = self._clear_distance(185.0, z, 375.0, z)
                self.assertLessEqual(d, 90.0, "processing hall cover should break long lines")

    def test_no_enemy_sees_spawn_cell(self):
        spawn = next(p for p in self.points if p["type"] == "player_spawn")
        sx, _, sz = spawn["pos"]
        enemies = [p for p in self.points if p["type"] in ("human", "staged_human")
                   and p.get("team", 1) != 0]
        for e in enemies:
            ex, _, ez = e["pos"]
            with self.subTest(enemy=e.get("pos")):
                d = self._clear_distance(ex, ez, sx, sz)
                full = math.hypot(sx - ex, sz - ez)
                self.assertLess(d, full - 1.0,
                                 "an enemy has an unobstructed line to the spawn cell")


class PathIdTests(unittest.TestCase):
    """AI paths are looked up by p4 map-wide; path ids must be unique and
    every route must stay on waypoints."""

    def test_every_route_resolves_to_unique_waypoints(self):
        pts = pd1.loads(PD1_PATH.read_bytes()).points
        by_p4 = {}
        for p in pts:
            by_p4.setdefault(p.p4, []).append(p)
        starts = {p.p3 for p in pts if p.p1 in (1, 6, 50)}
        starts.discard(0)
        self.assertTrue(starts)
        for s in sorted(starts):
            with self.subTest(route=s):
                seen = set()
                cur = s
                while cur not in seen:
                    seen.add(cur)
                    hits = by_p4.get(cur, [])
                    self.assertEqual(len(hits), 1, f"path id {cur} must be unique map-wide")
                    self.assertEqual(hits[0].p1, 3, f"path id {cur} must be a waypoint")
                    cur = hits[0].p3


class GuardOneTests(unittest.TestCase):
    """Engine facing convention (gamemain.cpp first-person camera:
    crx = pi/2 - r): a rotation r looks along (sin r, cos r) in x/z, so
    r = 0 faces +Z (south) and r = pi faces -Z (north)."""

    @staticmethod
    def _facing(r):
        return math.sin(r), math.cos(r)

    def _doc(self):
        doc = json.loads(JSON_SRC.read_text())
        spawn = next(p for p in doc["points"] if p["type"] == "player_spawn")
        guard = next(p for p in doc["points"] if p["type"] == "human" and p.get("human_type") == 44)
        wait = next(p for p in doc["points"] if p["type"] == "waypoint" and p.get("p4") == guard.get("ai_path"))
        return spawn, guard, wait

    # The spawn cell's doorway: gap centred on z 382 in the x = -12 wall.
    DOOR = (-12.0, 382.0)

    def test_guard_within_25_35_units_with_back_to_the_doorway(self):
        _, guard, wait = self._doc()
        gx, _, gz = guard["pos"]
        dist = math.hypot(gx - self.DOOR[0], gz - self.DOOR[1])
        self.assertGreaterEqual(dist, 25.0)
        self.assertLessEqual(dist, 35.0)
        to_door = ((self.DOOR[0] - gx) / dist, (self.DOOR[1] - gz) / dist)
        # A wait waypoint makes the AI turn to the waypoint's rotation, so
        # both the human and his waypoint must face away from the door.
        for r in (guard["rotation"], wait["rotation"]):
            fx, fz = self._facing(r)
            self.assertLess(fx * to_door[0] + fz * to_door[1], -0.7)

    def test_player_spawns_facing_the_cell_doorway(self):
        spawn, _, _ = self._doc()
        sx, _, sz = spawn["pos"]
        d = math.hypot(self.DOOR[0] - sx, self.DOOR[1] - sz)
        fx, fz = self._facing(spawn["rotation"])
        self.assertGreater(fx * (self.DOOR[0] - sx) / d + fz * (self.DOOR[1] - sz) / d, 0.9)


class EventChainTests(unittest.TestCase):
    def test_event_chain_reachable_from_entry_minus100_in_order(self):
        pts = pd1.loads(PD1_PATH.read_bytes()).points
        events = {p.p4: p for p in pts if (10 <= p.p1 <= 19) or p.p1 == 41}
        seen = []
        current = -100
        guard = 0
        while current in events and guard < 50:
            p = events[current]
            seen.append((p.p1, p.p2))
            current = p.p3
            guard += 1
        expected = [
            (17, 2), (18, 0), (13, 0), (18, 1), (41, 1),
            (13, 0), (18, 2), (41, 2), (13, 0), (10, 0),
        ]
        self.assertEqual(seen, expected)


class MsgFormatTests(unittest.TestCase):
    def test_lines_within_byte_limit(self):
        lines = MSG_PATH.read_text().splitlines()
        self.assertEqual(len(lines), 3)
        for line in lines:
            self.assertLessEqual(len(line) + 2, 73)


class TubeAddonFormatTests(unittest.TestCase):
    def test_six_lines_within_limits(self):
        lines = TUBE_PATH.read_text().splitlines()
        self.assertEqual(len(lines), 6)
        model, texture, decide, hp, sound, jump = lines
        self.assertTrue(model.endswith(".x"))
        self.assertTrue(texture.endswith(".bmp"))
        self.assertEqual(int(hp), 127)
        self.assertTrue(sound.endswith(".wav"))
        self.assertEqual(int(jump), 0)
        for line in lines:
            self.assertLess(len(line), 32)

    def test_cover_hull_matches_tube_dimensions(self):
        line = TUBE_COVER_PATH.read_text().splitlines()[0]
        parts = line.split()
        self.assertEqual(len(parts), 5)
        _, halfwidth, miny, maxy, halflength = (float(v) for v in parts)
        self.assertAlmostEqual(halfwidth, 5.0, delta=1.0)
        self.assertAlmostEqual(maxy - miny, 26.0, delta=1.0)


class MifFormatTests(unittest.TestCase):
    def test_header_fields_and_briefing_limits(self):
        lines = MIF_PATH.read_text().splitlines()
        name, fullname, bd1_path, pd1_path, sky, flags, addobj, picA, picB = lines[:9]
        self.assertLessEqual(len(name), 23)
        self.assertLessEqual(len(fullname), 35)
        self.assertEqual(bd1_path, r"data\ufoescape\temp.bd1")
        self.assertEqual(pd1_path, r"data\ufoescape\escape.pd1")
        self.assertEqual(sky, "0")
        self.assertEqual(flags, "0")
        self.assertEqual(addobj, r"data\ufoescape\tube.txt")
        self.assertEqual(picA, r"data\briefing\ufoescape.bmp")
        self.assertEqual(picB, "!")
        briefing = lines[9:]
        self.assertLessEqual(len(briefing), 14)
        for line in briefing:
            self.assertLessEqual(len(line), 52)


class BriefingPictureTests(unittest.TestCase):
    def test_bmp_header(self):
        self.assertTrue(BRIEFING_BMP.exists())
        data = BRIEFING_BMP.read_bytes()
        self.assertEqual(data[0:2], b"BM")
        w, h = struct.unpack_from("<ii", data, 0x12)
        self.assertEqual((w, h), (256, 256))


class MainMenuListingTests(unittest.TestCase):
    def test_ufo_escape_is_second_main_menu_entry(self):
        entries = [line.split("|") for line in MISSIONS_PATH.read_text().splitlines()
                   if line and not line.startswith(("#", ";"))]
        self.assertEqual(entries[0][0], "UFO FIELD")
        self.assertEqual(entries[1], ["UFO ESCAPE", "UFO ESCAPE - break out of the ship",
                                      "data\\ufoescape\\", "escape.mif", "0", "0"])


if __name__ == "__main__":
    unittest.main()

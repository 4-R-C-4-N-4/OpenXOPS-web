"""Tests for the original starter mission content (Phase C of
docs/OPEN-ASSETS-SOW.md): deterministic rebuild of the committed
starter/map10 .bd1/.pd1 binaries from their JSON source, basic
in-bounds / spawn-on-floor sanity checks, limit checks, and a
well-formed-ness check on the data/missions.txt line.
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.mapgen import author, bd1, pd1  # noqa: E402

STARTER_JSON = REPO_ROOT / "tools" / "mapgen" / "examples" / "starter_compound.json"
MAP10_JSON = REPO_ROOT / "tools" / "mapgen" / "examples" / "map10_demo.json"
STARTER_BD1 = REPO_ROOT / "assets" / "pack" / "data" / "starter" / "temp.bd1"
STARTER_PD1 = REPO_ROOT / "assets" / "pack" / "data" / "starter" / "start.pd1"
STARTER_TXT = REPO_ROOT / "assets" / "pack" / "data" / "starter" / "start.txt"
MAP10_BD1 = REPO_ROOT / "assets" / "pack" / "data" / "map10" / "temp.bd1"
MAP10_PD1 = REPO_ROOT / "assets" / "pack" / "data" / "map10" / "op.pd1"
MISSIONS_TXT = REPO_ROOT / "assets" / "pack" / "data" / "missions.txt"
HARNESS_SRC_DIR = REPO_ROOT / "tests" / "mapgen"
SRC_DIR = REPO_ROOT / "src"


def _bounds(blocks):
    xs = [v for b in blocks for v in b.x]
    ys = [v for b in blocks for v in b.y]
    zs = [v for b in blocks for v in b.z]
    return min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)


def _player_spawn(points):
    for p in points:
        if p.p1 == pd1.P1_HUMAN2 and p.p4 == 0:
            return p
    return None


class StarterScaleTests(unittest.TestCase):
    def test_geometry_matches_twenty_unit_humans(self):
        doc = json.loads(STARTER_JSON.read_text())
        # Engine HUMAN_HEIGHT=20 and HUMAN_MAPCOLLISION_R=2.8 (object.h).
        north = next(b for b in doc['blocks'] if b.get('comment') == 'perimeter north wall')
        self.assertGreaterEqual(north['max'][1] - north['min'][1], 30)
        self.assertGreaterEqual(north['max'][0] - north['min'][0], 300)

    def test_pickup_models_are_in_the_free_pack(self):
        import re
        doc = json.loads(STARTER_JSON.read_text())
        parameters = (SRC_DIR / 'parameter.cpp').read_text()
        for point in doc['points']:
            if point['type'] != 'weapon_pickup':
                continue
            wid = point['weapon_id']
            match = re.search(r'Weapon\[' + str(wid) + r'\]\.model\s*=\s*"([^"]+)"', parameters)
            self.assertIsNotNone(match, wid)
            model = match.group(1).removeprefix('./')
            self.assertTrue((REPO_ROOT / 'assets/pack' / model).is_file(), model)

    def test_pickups_are_not_buried_in_cover(self):
        doc = json.loads(STARTER_JSON.read_text())
        for point in doc['points']:
            if point['type'] != 'weapon_pickup':
                continue
            x, y, z = point['pos']; y += 0.1
            for box in doc['blocks']:
                lo, hi = box['min'], box['max']
                inside = lo[0] < x < hi[0] and lo[1] < y < hi[1] and lo[2] < z < hi[2]
                self.assertFalse(inside, (point, box.get('comment')))

    def test_spawn_has_full_height_cover_from_front(self):
        doc = json.loads(STARTER_JSON.read_text())
        spawn = next(p for p in doc['points'] if p['type'] == 'player_spawn')['pos']
        cover = next(b for b in doc['blocks'] if b.get('comment') == 'spawn protective barricade')
        self.assertGreater(cover['max'][1], spawn[1] + 20)
        self.assertLess(cover['max'][2], spawn[2])
        self.assertGreater(cover['max'][0] - cover['min'][0], 50)


class StarterMapDeterminismTests(unittest.TestCase):
    """The committed .bd1/.pd1 binaries must match a fresh build from
    the committed JSON source (tools.mapgen.author.build), so the
    binaries are reproducible, not hand-edited drift."""

    def test_starter_bd1_matches_json_source(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        self.assertEqual(result.bd1_bytes, STARTER_BD1.read_bytes())

    def test_starter_pd1_matches_json_source(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        self.assertEqual(result.pd1_bytes, STARTER_PD1.read_bytes())

    def test_map10_bd1_matches_json_source(self):
        doc = json.loads(MAP10_JSON.read_text())
        result = author.build(doc)
        self.assertEqual(result.bd1_bytes, MAP10_BD1.read_bytes())

    def test_map10_pd1_matches_json_source(self):
        doc = json.loads(MAP10_JSON.read_text())
        result = author.build(doc)
        self.assertEqual(result.pd1_bytes, MAP10_PD1.read_bytes())


class StarterMapLimitsTests(unittest.TestCase):
    def test_starter_block_count_within_max_and_sow_range(self):
        doc = json.loads(STARTER_JSON.read_text())
        self.assertLessEqual(len(doc["blocks"]), bd1.MAX_BLOCKS)
        self.assertGreaterEqual(len(doc["blocks"]), 40)
        self.assertLessEqual(len(doc["blocks"]), 120)

    def test_starter_point_count_within_max_points(self):
        doc = json.loads(STARTER_JSON.read_text())
        self.assertLessEqual(len(doc["points"]), pd1.MAX_POINTS)

    def test_starter_texture_slots_within_limit(self):
        doc = json.loads(STARTER_JSON.read_text())
        self.assertLessEqual(len(doc["textures"]), bd1.TOTAL_BLOCKTEXTURE)

    def test_map10_block_count_within_max(self):
        doc = json.loads(MAP10_JSON.read_text())
        self.assertLessEqual(len(doc["blocks"]), bd1.MAX_BLOCKS)


class StarterMapContentTests(unittest.TestCase):
    """Sanity checks on the authored content itself: player spawn
    on/above the floor and inside the map bounds, at least one
    weapon, several enemies/waypoints, and one event point."""

    def setUp(self):
        self.doc = json.loads(STARTER_JSON.read_text())
        self.result = author.build(self.doc)
        self.bd1_file = bd1.loads(self.result.bd1_bytes)
        self.pd1_file = pd1.loads(self.result.pd1_bytes)

    def test_player_spawn_present_and_inside_bounds(self):
        spawn = _player_spawn(self.pd1_file.points)
        self.assertIsNotNone(spawn, "no player_spawn (HUMAN2 with p4==0) point found")
        xmin, xmax, ymin, ymax, zmin, zmax = _bounds(self.bd1_file.blocks)
        self.assertTrue(xmin <= spawn.x <= xmax, "spawn.x outside map bounds")
        self.assertTrue(zmin <= spawn.z <= zmax, "spawn.z outside map bounds")

    def test_player_spawn_on_or_above_floor(self):
        # The authored slab spans y=-1..0; keep the player's feet above it.
        spawn = _player_spawn(self.pd1_file.points)
        self.assertGreaterEqual(spawn.y, 0.0)

    def test_player_has_configured_nonempty_default_weapon(self):
        # HUMANINFO resolves Human[2], whose slot 0 is the equipped M92F.
        spawn = _player_spawn(self.pd1_file.points)
        self.assertIsNotNone(spawn)
        self.assertEqual(spawn.p2, 1)
        info = next(p for p in self.pd1_file.points if p.p1 == pd1.P1_HUMANINFO and p.p4 == spawn.p2)
        self.assertEqual((info.p2, info.p3), (2, 0))
        parameter_cpp = (SRC_DIR / "parameter.cpp").read_text()
        self.assertIn("Human[2].Weapon[0] = 3;", parameter_cpp)
        self.assertIn('Weapon[3].name = "M92F"', parameter_cpp)

    def test_has_multiple_enemies_with_waypoints(self):
        enemies = [p for p in self.pd1_file.points if p.p1 == pd1.P1_HUMAN2 and p.p4 != 0]
        waypoints = [p for p in self.pd1_file.points if p.p1 == pd1.P1_AIPATH]
        self.assertGreaterEqual(len(enemies), 4)
        self.assertLessEqual(len(enemies), 6)
        self.assertGreaterEqual(len(waypoints), 1)

    def test_has_weapon_pickups(self):
        weapons = [p for p in self.pd1_file.points if p.p1 == pd1.P1_WEAPON]
        self.assertGreaterEqual(len(weapons), 1)

    def test_has_goal_event(self):
        events = [p for p in self.pd1_file.points if pd1.P1_EVENT_MIN <= p.p1 <= pd1.P1_EVENT_MAX]
        self.assertGreaterEqual(len(events), 1)


class StarterBriefingTextTests(unittest.TestCase):
    """start.txt must follow MIFInterface::LoadDefaultTextFiledata's
    plain-.txt format (src/datafile.cpp:789-838): line1/line2 are
    picture names or '!' for none, line3 is the sky number, then up
    to 17 lines (each read with fgets(str,50,fp), so >=1 char but
    under the 50-byte buffer) of briefing text."""

    def setUp(self):
        self.lines = STARTER_TXT.read_text().splitlines()

    def test_has_picture_and_sky_header(self):
        self.assertGreaterEqual(len(self.lines), 4)
        self.assertEqual(self.lines[0], "!")  # no picture A
        self.assertEqual(self.lines[1], "!")  # no picture B
        self.assertTrue(self.lines[2].strip().lstrip("-").isdigit())

    def test_briefing_lines_fit_fgets_buffer(self):
        body = self.lines[3:]
        self.assertLessEqual(len(body), 17)
        for line in body:
            # fgets(str, 50, fp) keeps at most 49 chars + NUL before
            # the newline; longer lines would silently split across
            # two of the 17 slots instead of rendering as one line.
            self.assertLessEqual(len(line), 49, f"line too long for one fgets(50) call: {line!r}")


class MissionsTxtFormatTests(unittest.TestCase):
    def test_starter_line_well_formed(self):
        lines = [l for l in MISSIONS_TXT.read_text().splitlines() if l.strip()]
        starter_lines = [l for l in lines if l.startswith("STARTER|")]
        self.assertEqual(len(starter_lines), 1)
        fields = starter_lines[0].split("|")
        self.assertEqual(len(fields), 6, "expected name|fullname|directory|txtbase|collision|screen")
        name, fullname, directory, txtbase, collision, screen = fields
        self.assertEqual(name, "STARTER")
        self.assertTrue(fullname)
        self.assertEqual(directory, "data\\starter\\")
        self.assertEqual(txtbase, "start")
        self.assertIn(collision, ("0", "1", "2", "3"))
        self.assertIn(screen, ("0", "1"))


@unittest.skipUnless(shutil.which("g++"), "g++ not available; skipping engine-loader proof")
class EngineLoaderProofTests(unittest.TestCase):
    """Loads the committed starter and map10 binaries through the
    real, unmodified src/datafile.cpp loaders (same harness as
    tools/tests/test_mapgen.py)."""

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = Path(tempfile.mkdtemp(prefix="starter_harness_"))
        cls.binary = cls.tmpdir / "load_harness"
        objs = []
        sources = [
            (HARNESS_SRC_DIR / "load_harness.cpp", []),
            (HARNESS_SRC_DIR / "harness_support.cpp", []),
            (SRC_DIR / "datafile.cpp", []),
            (SRC_DIR / "platform" / "vfs.cpp", []),
        ]
        for src, extra in sources:
            obj = cls.tmpdir / (src.stem + ".o")
            cmd = ["g++", "-std=c++17", "-I", str(SRC_DIR), "-c", str(src), "-o", str(obj)] + extra
            proc = subprocess.run(cmd, capture_output=True, text=True)
            if proc.returncode != 0:
                raise RuntimeError(f"compile failed for {src}:\n{proc.stdout}\n{proc.stderr}")
            objs.append(str(obj))
        link_cmd = ["g++", "-std=c++17", "-o", str(cls.binary)] + objs + ["-lz"]
        proc = subprocess.run(link_cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(f"link failed:\n{proc.stdout}\n{proc.stderr}")

        cls.collision_binary = cls.tmpdir / 'collision_harness'
        collision_cmd = ['g++', '-std=c++17', '-I', str(SRC_DIR),
                         str(HARNESS_SRC_DIR / 'collision_harness.cpp'),
                         str(SRC_DIR / 'collision.cpp')]
        collision_cmd += [o for o in objs if Path(o).stem != 'load_harness']
        collision_cmd += ['-lz', '-o', str(cls.collision_binary)]
        proc = subprocess.run(collision_cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(f'collision harness build failed:\n{proc.stderr}')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def test_starter_floor_uses_real_engine_collision(self):
        doc = json.loads(STARTER_JSON.read_text())
        spawn = next(p for p in doc['points'] if p['type'] == 'player_spawn')['pos']
        proc = subprocess.run([str(self.collision_binary), str(STARTER_BD1),
                               *map(str, spawn)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn('RAY_HIT block=0 face=0', proc.stdout)
        self.assertIn('OK', proc.stdout)

    def _run(self, bd1_path, pd1_path):
        proc = subprocess.run(
            [str(self.binary), str(bd1_path), str(pd1_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout.strip().splitlines()

    def test_engine_loader_reads_starter_compound(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        point_count = len(pd1.loads(result.pd1_bytes).points)
        lines = self._run(STARTER_BD1, STARTER_PD1)
        block_count = len(doc["blocks"])
        self.assertIn(f"BLOCKS {block_count}", lines)
        self.assertIn(f"POINTS {point_count}", lines)

    def test_engine_loader_reads_map10_demo(self):
        doc = json.loads(MAP10_JSON.read_text())
        result = author.build(doc)
        point_count = len(pd1.loads(result.pd1_bytes).points)
        lines = self._run(MAP10_BD1, MAP10_PD1)
        block_count = len(doc["blocks"])
        self.assertIn(f"BLOCKS {block_count}", lines)
        self.assertIn(f"POINTS {point_count}", lines)


if __name__ == "__main__":
    unittest.main()

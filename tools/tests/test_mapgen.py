"""Tests for tools/mapgen: byte-exact round trips, field checks against
loader expectations, invalid-input rejection, and (when g++ is
available) an engine-loader proof via tests/mapgen/load_harness.cpp.
"""

import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.mapgen import author, bd1, pd1  # noqa: E402

STARTER_JSON = REPO_ROOT / "tools" / "mapgen" / "examples" / "starter.json"
HARNESS_SRC_DIR = REPO_ROOT / "tests" / "mapgen"
SRC_DIR = REPO_ROOT / "src"


def make_simple_bd1():
    tex = ["wall.bmp", "floor.bmp"]
    block = bd1.box_block([0, 0, 0], [2, 2, 2], [0, 1, 0, 1, 0, 1], uv_scale=(2, 3))
    return bd1.BD1File(textures=tex, blocks=[block, block])


def make_simple_pd1():
    pts = [
        pd1.Point(1.0, 2.0, 3.0, 0.5, p1=pd1.P1_HUMAN2, p2=0, p3=0, p4=0),
        pd1.Point(-1.0, 0.0, 4.0, 0.0, p1=pd1.P1_WEAPON, p2=1, p3=60, p4=0),
        pd1.Point(0.0, 0.0, 0.0, 0.0, p1=10, p2=5, p3=-5, p4=127),
    ]
    return pd1.PD1File(points=pts)


class WallTextureOrientationTests(unittest.TestCase):
    """Side faces must show textures upright and unmirrored: v=0 (the top
    row of the image) on the top edge, u increasing to the viewer's right."""

    # blockdataface(): side face -> vertex ids in listed order and uv slots
    SIDE_FACES = {2: (5, 4, 0, 1), 3: (6, 5, 1, 2), 4: (7, 6, 2, 3), 5: (4, 7, 3, 0)}

    def test_side_faces_upright(self):
        block = bd1.box_block((0, 0, 0), (10, 20, 30), [1] * 6, uv_scale=(2.0, 3.0))
        for face, vids in self.SIDE_FACES.items():
            mat = block.faces[face]
            for pos, vid in enumerate(vids):
                slot = bd1.FACE_UVSLOT_MAP[pos]
                top = block.y[vid] == 20
                self.assertEqual(mat.v[slot], 0.0 if top else 3.0, (face, vid))
            # listed order is bottom-right then bottom-left seen from outside
            self.assertGreater(mat.u[bd1.FACE_UVSLOT_MAP[0]], mat.u[bd1.FACE_UVSLOT_MAP[1]])


class BD1RoundTripTests(unittest.TestCase):
    def test_round_trip_bytes_exact(self):
        f = make_simple_bd1()
        data = bd1.dumps(f)
        f2 = bd1.loads(data)
        data2 = bd1.dumps(f2)
        self.assertEqual(data, data2)

    def test_header_layout(self):
        f = make_simple_bd1()
        data = bd1.dumps(f)
        # 10 * 31-byte texture slots = 310 bytes, then 2-byte count.
        self.assertEqual(data[0:8], b"wall.bmp")
        self.assertEqual(data[8], 0)  # NUL terminator within the 31-byte field
        self.assertEqual(len(data[0:310]), 310)
        (count,) = struct.unpack_from("<H", data, 310)
        self.assertEqual(count, 2)

    def test_block_record_size(self):
        f = make_simple_bd1()
        data = bd1.dumps(f)
        expected = bd1.HEADER_SIZE + 2 + len(f.blocks) * bd1.BLOCK_RECORD_SIZE
        self.assertEqual(len(data), expected)
        self.assertEqual(bd1.BLOCK_RECORD_SIZE, 316)

    def test_face_vertex_and_textureid_layout(self):
        # Spot-check the face->vertex table against datafile.cpp:324-381
        # and the textureID byte offset against datafile.cpp:155.
        self.assertEqual(bd1.FACE_VERTEX_MAP[0], (1, 0, 3, 2))  # top
        self.assertEqual(bd1.FACE_VERTEX_MAP[5], (4, 7, 3, 0))  # left
        self.assertEqual(bd1.FACE_UVSLOT_MAP, (2, 3, 0, 1))

        block = bd1.box_block([0, 0, 0], [1, 1, 1], [3, 4, 5, 6, 7, 8])
        f = bd1.BD1File(textures=[], blocks=[block])
        data = bd1.dumps(f)
        block_start = bd1.HEADER_SIZE + 2
        texid_start = block_start + 288
        for j, expected_id in enumerate((3, 4, 5, 6, 7, 8)):
            got = struct.unpack_from("<b", data, texid_start + j * 4)[0]
            self.assertEqual(got, expected_id, f"face {j}")

    def test_loads_matches_struct_fields(self):
        f = make_simple_bd1()
        f2 = bd1.loads(bd1.dumps(f))
        self.assertEqual(f2.textures[:2], ["wall.bmp", "floor.bmp"])
        self.assertEqual(len(f2.blocks), 2)
        self.assertEqual(f2.blocks[0].faces[0].texture_id, 0)
        self.assertEqual(f2.blocks[0].x, f.blocks[0].x)
        self.assertEqual(f2.blocks[0].faces[0].u, f.blocks[0].faces[0].u)

    def test_max_blocks_rejected_on_build(self):
        block = bd1.box_block([0, 0, 0], [1, 1, 1], [0] * 6)
        with self.assertRaises(bd1.BD1Error):
            bd1.BD1File(textures=[], blocks=[block] * (bd1.MAX_BLOCKS + 1))

    def test_loader_clamps_oversized_count_header(self):
        # Hand-craft a header claiming more blocks than MAX_BLOCKS, but
        # back it with exactly MAX_BLOCKS+1 worth of block bytes; the
        # real loader clamps the *count* to MAX_BLOCKS and never reads
        # the extra trailing block (datafile.cpp:135).
        block = bd1.box_block([0, 0, 0], [1, 1, 1], [0] * 6)
        f = bd1.BD1File(textures=[], blocks=[block] * bd1.MAX_BLOCKS)
        data = bytearray(bd1.dumps(f))
        extra = struct.pack("<72f", *([0.0] * 72)) + bytes(28)
        data += extra  # one more block's worth of bytes than the real count
        struct.pack_into("<H", data, bd1.HEADER_SIZE, bd1.MAX_BLOCKS + 50)
        f2 = bd1.loads(bytes(data))
        self.assertEqual(len(f2.blocks), bd1.MAX_BLOCKS)

    def test_texture_name_too_long_rejected(self):
        with self.assertRaises(bd1.BD1Error):
            bd1.BD1File(textures=["x" * 31], blocks=[])

    def test_too_many_texture_slots_rejected(self):
        with self.assertRaises(bd1.BD1Error):
            bd1.BD1File(textures=["t"] * (bd1.TOTAL_BLOCKTEXTURE + 1), blocks=[])

    def test_block_needs_8_vertices_and_6_faces(self):
        with self.assertRaises(bd1.BD1Error):
            bd1.Block(x=(0,) * 7, y=(0,) * 8, z=(0,) * 8, faces=[bd1.FaceMaterial()] * 6)
        with self.assertRaises(bd1.BD1Error):
            bd1.Block(x=(0,) * 8, y=(0,) * 8, z=(0,) * 8, faces=[bd1.FaceMaterial()] * 5)

    def test_texture_id_out_of_signed_byte_range_rejected(self):
        with self.assertRaises(bd1.BD1Error):
            bd1.FaceMaterial(texture_id=128)
        with self.assertRaises(bd1.BD1Error):
            bd1.FaceMaterial(texture_id=-129)


class PD1RoundTripTests(unittest.TestCase):
    def test_round_trip_bytes_exact(self):
        f = make_simple_pd1()
        data = pd1.dumps(f)
        f2 = pd1.loads(data)
        data2 = pd1.dumps(f2)
        self.assertEqual(data, data2)

    def test_record_layout(self):
        f = make_simple_pd1()
        data = pd1.dumps(f)
        (count,) = struct.unpack_from("<H", data, 0)
        self.assertEqual(count, 3)
        self.assertEqual(len(data), 2 + 3 * pd1.RECORD_SIZE)
        self.assertEqual(pd1.RECORD_SIZE, 20)
        x, y, z, r = struct.unpack_from("<4f", data, 2)
        self.assertEqual((x, y, z, r), (1.0, 2.0, 3.0, 0.5))
        p1, p2, p3, p4 = struct.unpack_from("<4b", data, 2 + 16)
        self.assertEqual((p1, p2, p3, p4), (pd1.P1_HUMAN2, 0, 0, 0))

    def test_loader_clamps_oversized_count_header(self):
        # Same idea as the .bd1 clamp test: back the oversized count
        # with MAX_POINTS+1 real records so the clamp (not truncation)
        # is what's being exercised (datafile.cpp:438).
        pt = pd1.Point(0, 0, 0, 0, p1=1)
        f = pd1.PD1File(points=[pt] * pd1.MAX_POINTS)
        data = bytearray(pd1.dumps(f))
        data += struct.pack("<4f", 9.0, 9.0, 9.0, 0.0) + struct.pack("<4b", 1, 0, 0, 0)
        struct.pack_into("<H", data, 0, pd1.MAX_POINTS + 10)
        f2 = pd1.loads(bytes(data))
        self.assertEqual(len(f2.points), pd1.MAX_POINTS)

    def test_max_points_rejected_on_build(self):
        pt = pd1.Point(0, 0, 0, 0, p1=1)
        with self.assertRaises(pd1.PD1Error):
            pd1.PD1File(points=[pt] * (pd1.MAX_POINTS + 1))

    def test_param_out_of_signed_byte_range_rejected(self):
        with self.assertRaises(pd1.PD1Error):
            pd1.Point(0, 0, 0, 0, p1=200)
        with self.assertRaises(pd1.PD1Error):
            pd1.Point(0, 0, 0, 0, p1=1, p2=-200)

    def test_negative_params_round_trip(self):
        pt = pd1.Point(0, 0, 0, 0, p1=3, p2=-1, p3=-128, p4=127)
        f = pd1.PD1File(points=[pt])
        f2 = pd1.loads(pd1.dumps(f))
        got = f2.points[0]
        self.assertEqual((got.p1, got.p2, got.p3, got.p4), (3, -1, -128, 127))


class AuthorFormatTests(unittest.TestCase):
    def test_starter_json_builds(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        self.assertGreater(len(result.bd1_bytes), 0)
        self.assertGreater(len(result.pd1_bytes), 0)
        f = bd1.loads(result.bd1_bytes)
        self.assertEqual(len(f.blocks), 7)
        self.assertEqual(f.textures[:3], ["wall.bmp", "floor.bmp", "crate.bmp"])
        p = pd1.loads(result.pd1_bytes)
        # 6 authored points + 2 synthesized HUMANINFO companions, one
        # per distinct (human_type, team) pair: player (0,0) and the
        # two enemies sharing (2,2) (see author._synthesize_human_info).
        self.assertEqual(len(p.points), 8)
        self.assertEqual(sum(1 for pt in p.points if pt.p1 == pd1.P1_HUMANINFO), 2)

    def test_build_dump_build_is_byte_stable(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        bd1_doc = author.dump_bd1(result.bd1_bytes)
        pd1_doc = author.dump_pd1(result.pd1_bytes)
        rebuilt = author.build({
            "textures": bd1_doc["textures"],
            "blocks": bd1_doc["blocks"],
            "points": pd1_doc["points"],
        })
        self.assertEqual(rebuilt.bd1_bytes, result.bd1_bytes)
        self.assertEqual(rebuilt.pd1_bytes, result.pd1_bytes)

    def test_player_spawn_and_human_point_kinds(self):
        doc = {
            "textures": [],
            "blocks": [],
            "points": [
                {"type": "player_spawn", "pos": [0, 0, 0]},
                {"type": "human", "pos": [1, 0, 0], "team": 2, "human_type": 5, "ai_path": 3},
            ],
        }
        result = author.build(doc)
        p = pd1.loads(result.pd1_bytes)
        self.assertEqual((p.points[0].p1, p.points[0].p4), (pd1.P1_HUMAN2, 0))
        self.assertEqual((p.points[1].p1, p.points[1].p3, p.points[1].p4),
                          (pd1.P1_HUMAN2, 3, 2))
        # p2 is now a HUMANINFO foreign key, not the raw human_type;
        # resolve it through the synthesized companion point
        # (objectmanager.cpp:1465's p1==HUMANINFO && p4==HUMAN2.p2 lookup).
        info = next(pt for pt in p.points if pt.p1 == pd1.P1_HUMANINFO
                    and pt.p4 == p.points[1].p2)
        self.assertEqual((info.p2, info.p3), (5, 2))
        # And the decompiled JSON round-trips back to the authored human_type.
        dumped = author.dump_pd1(result.pd1_bytes)
        human_entry = next(e for e in dumped["points"] if e["type"] == "human")
        self.assertEqual(human_entry["human_type"], 5)

    def test_human_team_zero_rejected(self):
        doc = {"points": [{"type": "human", "pos": [0, 0, 0], "team": 0}]}
        with self.assertRaises(author.AuthorError):
            author.build(doc)

    def test_event_id_out_of_range_rejected(self):
        doc = {"points": [{"type": "event", "pos": [0, 0, 0], "event_id": 9}]}
        with self.assertRaises(author.AuthorError):
            author.build(doc)
        doc2 = {"points": [{"type": "event", "pos": [0, 0, 0], "event_id": 20}]}
        with self.assertRaises(author.AuthorError):
            author.build(doc2)

    def test_unknown_point_type_rejected(self):
        doc = {"points": [{"type": "bogus", "pos": [0, 0, 0]}]}
        with self.assertRaises(author.AuthorError):
            author.build(doc)

    def test_unknown_texture_name_rejected(self):
        doc = {
            "textures": ["a.bmp"],
            "blocks": [{"min": [0, 0, 0], "max": [1, 1, 1], "texture": "nope.bmp"}],
        }
        with self.assertRaises(author.AuthorError):
            author.build(doc)

    def test_raw_block_needs_8_vertices_and_6_faces(self):
        doc = {
            "textures": [],
            "blocks": [{"vertices": [[0, 0, 0]] * 7, "faces": [{} for _ in range(6)]}],
        }
        with self.assertRaises(author.AuthorError):
            author.build(doc)

    def test_block_needs_box_or_raw_shape(self):
        doc = {"blocks": [{"texture": "x"}]}
        with self.assertRaises(author.AuthorError):
            author.build(doc)


@unittest.skipUnless(shutil.which("g++"), "g++ not available; skipping engine-loader proof")
class EngineLoaderProofTests(unittest.TestCase):
    """Compiles tests/mapgen/load_harness.cpp against the real,
    unmodified src/datafile.cpp and runs it on files produced by
    tools/mapgen, checking the engine's own loader agrees on counts.
    """

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = Path(tempfile.mkdtemp(prefix="mapgen_harness_"))
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

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def test_engine_loader_reads_starter_map(self):
        doc = json.loads(STARTER_JSON.read_text())
        result = author.build(doc)
        bd1_path = self.tmpdir / "temp.bd1"
        pd1_path = self.tmpdir / "mission.pd1"
        bd1_path.write_bytes(result.bd1_bytes)
        pd1_path.write_bytes(result.pd1_bytes)

        proc = subprocess.run(
            [str(self.binary), str(bd1_path), str(pd1_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        lines = proc.stdout.strip().splitlines()
        self.assertIn("BLOCKS 7", lines)
        # 6 authored points + 2 synthesized HUMANINFO companions.
        self.assertIn("POINTS 8", lines)


if __name__ == "__main__":
    unittest.main()

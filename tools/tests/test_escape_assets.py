"""Tests for the UFO ESCAPE free-pack assets (tools/escapegen) built per
docs/MISSION-UFO-ESCAPE.md section 4 and docs/ASSET-CONTRACT.md.
"""
import json
import os
import struct
import sys
import unittest
import wave

from PIL import Image

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

CONTRACT_DIR = os.path.join(REPO_ROOT, "tools", "contract")
if CONTRACT_DIR not in sys.path:
    sys.path.insert(0, CONTRACT_DIR)
import check_x  # noqa: E402

from tools.escapegen import block_textures, rock, tube, sounds  # noqa: E402

DATA = os.path.join(REPO_ROOT, "assets", "pack", "data")
MAX_ROCK_POLYS = 60


def _bmp_header_ok(path):
    raw = open(path, "rb").read()
    assert raw[:2] == b"BM"
    biSize, width, height, planes, bpp, compression = struct.unpack_from("<IiiHHI", raw, 14)
    assert biSize == 40
    assert height > 0
    assert bpp == 8
    assert compression == 0
    off_bits = struct.unpack_from("<I", raw, 10)[0]
    assert off_bits == 14 + 40 + 1024


class BlockTextureTests(unittest.TestCase):
    NAMES = ["hull", "floor", "ceiling", "goo", "vat", "labpanel",
             "lightstrip", "hazard", "console", "bulkhead"]

    def test_exactly_ten_textures_defined(self):
        self.assertEqual(len(block_textures.GENERATORS), 10)
        for name in self.NAMES:
            self.assertIn(f"{name}.bmp", block_textures.GENERATORS)

    def test_all_ten_files_exist_and_valid_bmp8(self):
        for name in self.NAMES:
            path = os.path.join(DATA, "ufoescape", f"{name}.bmp")
            with self.subTest(name=name):
                self.assertTrue(os.path.isfile(path))
                _bmp_header_ok(path)
                self.assertEqual(list(Image.open(path).size), [256, 256])

    def test_tiles_wrap_seamlessly_at_edges(self):
        """Map tiles render with GL_REPEAT (ASSET-CONTRACT 9.1); left/right
        and top/bottom edge rows must match closely so no seam shows."""
        for name in self.NAMES:
            path = os.path.join(DATA, "ufoescape", f"{name}.bmp")
            image = Image.open(path).convert("RGB")
            w, h = image.size
            left = list(image.crop((0, 0, 1, h)).getdata())
            right = list(image.crop((w - 1, 0, w, h)).getdata())
            top = list(image.crop((0, 0, w, 1)).getdata())
            bottom = list(image.crop((0, h - 1, w, h)).getdata())

            def meandist(a, b):
                return sum(
                    sum((a[i][c] - b[i][c]) ** 2 for c in range(3)) ** 0.5 for i in range(len(a))
                ) / len(a)

            with self.subTest(name=name):
                self.assertLess(meandist(left, right), 60)
                self.assertLess(meandist(top, bottom), 60)

    def test_deterministic_regeneration(self):
        for name in self.NAMES:
            fn = block_textures.GENERATORS[f"{name}.bmp"]
            a = fn()
            b = fn()
            self.assertEqual(list(a.getdata()), list(b.getdata()))

    def test_goo_is_bright_saturated_green(self):
        path = os.path.join(DATA, "ufoescape", "goo.bmp")
        image = Image.open(path).convert("RGB")
        pixels = list(image.getdata())
        mean = tuple(sum(p[i] for p in pixels) / len(pixels) for i in range(3))
        self.assertGreater(mean[1], mean[0] + 20, "goo should read green-dominant")
        self.assertGreater(mean[1], mean[2] + 20)
        self.assertGreater(mean[1], 90, "goo should be bright, not muddy")

    def test_lightstrip_reads_bright_emissive(self):
        path = os.path.join(DATA, "ufoescape", "lightstrip.bmp")
        image = Image.open(path).convert("RGB")
        pixels = list(image.getdata())
        brightest = max(max(p) for p in pixels)
        self.assertGreater(brightest, 200, "lightstrip must have an emissive-bright hot core")

    def test_hazard_has_strong_yellow_black_contrast(self):
        path = os.path.join(DATA, "ufoescape", "hazard.bmp")
        image = Image.open(path).convert("RGB")
        pixels = list(image.getdata())
        bright = sum(1 for p in pixels if sum(p) > 400)
        dark = sum(1 for p in pixels if sum(p) < 150)
        self.assertGreater(bright, len(pixels) * 0.15)
        self.assertGreater(dark, len(pixels) * 0.15)


class RockModelTests(unittest.TestCase):
    def test_model_exists_and_parses(self):
        path = os.path.join(DATA, "model", "weapon", "rock.x")
        self.assertTrue(os.path.isfile(path))
        meshes, errors, warns = check_x.parse(path)
        self.assertEqual(errors, [], f"rock.x: {errors}")
        self.assertGreater(len(meshes), 0)

    def test_poly_budget(self):
        mesh = rock.build_rock_mesh()
        self.assertGreater(mesh.poly_count, 0)
        self.assertLessEqual(mesh.poly_count, MAX_ROCK_POLYS)

    def test_uv_count_matches_vertex_count_and_in_range(self):
        path = os.path.join(DATA, "model", "weapon", "rock.x")
        meshes, errors, warns = check_x.parse(path)
        for mesh in meshes:
            self.assertEqual(len(mesh["uvs"]), len(mesh["verts"]))
            for u, v in mesh["uvs"]:
                self.assertGreaterEqual(u, -1e-9)
                self.assertLessEqual(u, 1 + 1e-9)
                self.assertGreaterEqual(v, -1e-9)
                self.assertLessEqual(v, 1 + 1e-9)

    def test_face_sizes_are_tri_or_quad(self):
        path = os.path.join(DATA, "model", "weapon", "rock.x")
        meshes, errors, warns = check_x.parse(path)
        for mesh in meshes:
            for f in mesh["faces"]:
                self.assertIn(len(f), (3, 4))

    def test_deterministic_regeneration(self):
        m1 = rock.build_rock_mesh()
        m2 = rock.build_rock_mesh()
        self.assertEqual(m1.verts, m2.verts)
        self.assertEqual(m1.faces, m2.faces)
        self.assertEqual(m1.uvs, m2.uvs)

    def test_pistol_sized(self):
        """Rock is bullet type 5 / weapon 27, anchored on the M92F template
        (docs/MISSION-UFO-ESCAPE.md section 4, docs/ASSET-CONTRACT.md 6.2)."""
        self.assertEqual(rock.ROCK_SPEC["template"], "M92F")
        self.assertEqual(rock.ROCK_SPEC["grip"], (2, 7, 57))
        self.assertEqual(rock.ROCK_SPEC["muzzle"], (0.0, 0.114, -0.229))
        mesh = rock.build_rock_mesh()
        verts = mesh.verts
        extent = max(max(abs(v[i]) for v in verts) for i in range(3))
        self.assertLess(extent, 0.3, "rock must be pistol/hand-prop sized in file units")

    def test_grey_blue_with_craters(self):
        img = rock.paint_rock_atlas()
        pixels = list(img.convert("RGB").getdata())
        mean = tuple(sum(p[i] for p in pixels) / len(pixels) for i in range(3))
        self.assertGreater(mean[2], mean[0] - 5, "rock should lean blue-grey (B >= R roughly)")
        darks = sum(1 for p in pixels if sum(p) < 300)
        self.assertGreater(darks, 10, "expected dark crater pixels")


class RockTextureTests(unittest.TestCase):
    def test_file_exists_and_bmp_valid(self):
        path = os.path.join(DATA, "model", "weapon", "rock.bmp")
        self.assertTrue(os.path.isfile(path))
        _bmp_header_ok(path)
        self.assertEqual(list(Image.open(path).size), [128, 128])

    def test_deterministic_regeneration(self):
        a = rock.paint_rock_atlas()
        b = rock.paint_rock_atlas()
        self.assertEqual(list(a.getdata()), list(b.getdata()))


class TubeModelTests(unittest.TestCase):
    def test_model_exists_and_parses(self):
        path = os.path.join(DATA, "model", "tube.x")
        self.assertTrue(os.path.isfile(path))
        meshes, errors, warns = check_x.parse(path)
        self.assertEqual(errors, [], f"tube.x: {errors}")
        self.assertGreater(len(meshes), 0)

    def test_segment_count_in_range(self):
        self.assertGreaterEqual(tube.SEGMENTS, 8)
        self.assertLessEqual(tube.SEGMENTS, 12)

    def test_height_and_radius(self):
        mesh, _ = tube.build_tube_mesh()
        verts = mesh.verts
        height = max(v[1] for v in verts) - min(v[1] for v in verts)
        radius = max((v[0] ** 2 + v[2] ** 2) ** 0.5 for v in verts)
        self.assertAlmostEqual(height, 5.2, delta=0.3)
        self.assertAlmostEqual(radius, 1.0, delta=0.15)
        # SMALLOBJECT_SCALE 5.0 (ASSET-CONTRACT 6.4): world = file * 5.
        world_h = height * 5.0
        self.assertGreaterEqual(world_h, 24.0)
        self.assertLessEqual(world_h, 28.0)

    def test_deterministic(self):
        a, _ = tube.build_tube_mesh()
        b, _ = tube.build_tube_mesh()
        self.assertEqual(a.verts, b.verts)
        self.assertEqual(a.faces, b.faces)

    def test_uv_count_matches_vertex_count_and_in_range(self):
        path = os.path.join(DATA, "model", "tube.x")
        meshes, errors, warns = check_x.parse(path)
        for mesh in meshes:
            self.assertEqual(len(mesh["uvs"]), len(mesh["verts"]))
            for u, v in mesh["uvs"]:
                self.assertGreaterEqual(u, -1e-9)
                self.assertLessEqual(u, 1 + 1e-9)
                self.assertGreaterEqual(v, -1e-9)
                self.assertLessEqual(v, 1 + 1e-9)


class TubeTextureTests(unittest.TestCase):
    def test_file_exists_and_bmp_valid(self):
        path = os.path.join(DATA, "model", "tube.bmp")
        self.assertTrue(os.path.isfile(path))
        _bmp_header_ok(path)
        self.assertEqual(list(Image.open(path).size), [128, 128])

    def test_liquid_reads_green(self):
        img = tube.paint_tube_atlas().convert("RGB")
        atlas = tube.tube_atlas()
        r = atlas.rects["glass"]
        pixels = list(img.crop((r.x0, r.y0, r.x1, r.y1)).getdata())
        mean = tuple(sum(p[i] for p in pixels) / len(pixels) for i in range(3))
        self.assertGreater(mean[1], mean[0] + 15)
        self.assertGreater(mean[1], mean[2] + 15)

    def test_deterministic_regeneration(self):
        a = tube.paint_tube_atlas()
        b = tube.paint_tube_atlas()
        self.assertEqual(list(a.getdata()), list(b.getdata()))


class SoundTests(unittest.TestCase):
    NAMES = ["throw", "glass"]

    def test_files_exist_and_valid_pcm16_mono(self):
        for name in self.NAMES:
            path = os.path.join(DATA, "sound", f"{name}.wav")
            with self.subTest(name=name):
                self.assertTrue(os.path.isfile(path))
                with wave.open(path, "rb") as wf:
                    self.assertEqual(wf.getnchannels(), 1)
                    self.assertEqual(wf.getsampwidth(), 2)
                    self.assertGreater(wf.getnframes(), 0)

    def test_non_silent(self):
        for name in self.NAMES:
            path = os.path.join(DATA, "sound", f"{name}.wav")
            with wave.open(path, "rb") as wf:
                frames = wf.readframes(wf.getnframes())
            import array
            samples = array.array("h")
            samples.frombytes(frames)
            peak = max(abs(s) for s in samples)
            with self.subTest(name=name):
                self.assertGreater(peak, 1000)

    def test_deterministic_regeneration(self):
        for name in self.NAMES:
            fn, _summary = sounds.RECIPES[name]
            a = fn(sounds.sx.rng_for(sounds.SEED, name))
            b = fn(sounds.sx.rng_for(sounds.SEED, name))
            self.assertEqual(sounds.sx.wav_bytes(a), sounds.sx.wav_bytes(b))


class ProvenanceTests(unittest.TestCase):
    """PROVENANCE-ADD.jsonl must hold a CC0 record, with the current sha256,
    for every UFO ESCAPE asset (docs/MISSION-UFO-ESCAPE.md section 4)."""

    PROV_PATH = os.path.join(REPO_ROOT, "assets", "provenance.jsonl")  # sidecar folded in at integration

    def setUp(self):
        with open(self.PROV_PATH) as fh:
            self.records = {r["path"]: r for r in (json.loads(l) for l in fh if l.strip())}

    def _expected(self):
        expected = set()
        for name in block_textures.GENERATORS:
            expected.add(f"data/ufoescape/{name}")
        expected.add("data/model/weapon/rock.x")
        expected.add("data/model/weapon/rock.bmp")
        expected.add("data/model/tube.x")
        expected.add("data/model/tube.bmp")
        for name in sounds.RECIPES:
            expected.add(f"data/sound/{name}.wav")
        return expected

    def test_every_new_asset_has_a_current_cc0_record(self):
        import hashlib
        for path in sorted(self._expected()):
            with self.subTest(path=path):
                rec = self.records.get(path)
                self.assertIsNotNone(rec, "missing provenance record")
                self.assertEqual(rec["license"], "CC0-1.0")
                for field in ("sha256", "generator", "date", "author", "prompt", "source_url"):
                    self.assertIn(field, rec)
                self.assertIsNone(rec["curated_by"])
                data = open(os.path.join(REPO_ROOT, "assets", "pack", path), "rb").read()
                self.assertEqual(rec["sha256"], hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    unittest.main()

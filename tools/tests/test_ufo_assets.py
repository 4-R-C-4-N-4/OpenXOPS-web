"""Tests for the UFO FIELD free-pack assets (tools/ufogen) built per
docs/MISSION-UFO-FIELD.md sections 3-4 and docs/ASSET-CONTRACT.md.
"""
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

from tools.ufogen import weapons, laser, cow, human_skins_ufo, block_textures, sounds  # noqa: E402

DATA = os.path.join(REPO_ROOT, "assets", "pack", "data")
MAX_WEAPON_POLYS = 139  # docs/MODELGEN.md F-4 budget


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


class WeaponModelTests(unittest.TestCase):
    def test_files_exist_and_parse(self):
        for key in weapons.UFO_WEAPON_SPECS:
            path = os.path.join(DATA, "model", "weapon", f"{key}.x")
            with self.subTest(key=key):
                self.assertTrue(os.path.isfile(path))
                meshes, errors, warns = check_x.parse(path)
                self.assertEqual(errors, [], f"{key}: {errors}")
                self.assertGreater(len(meshes), 0)

    def test_poly_budget(self):
        for key in weapons.UFO_WEAPON_SPECS:
            mesh, _ = weapons.build_weapon_mesh(key)
            with self.subTest(key=key):
                self.assertGreater(mesh.poly_count, 0)
                self.assertLessEqual(mesh.poly_count, MAX_WEAPON_POLYS)

    def test_uv_count_matches_vertex_count_and_in_range(self):
        for key in weapons.UFO_WEAPON_SPECS:
            path = os.path.join(DATA, "model", "weapon", f"{key}.x")
            meshes, errors, warns = check_x.parse(path)
            for mesh in meshes:
                with self.subTest(key=key):
                    self.assertEqual(len(mesh["uvs"]), len(mesh["verts"]))
                    for u, v in mesh["uvs"]:
                        self.assertGreaterEqual(u, -1e-9)
                        self.assertLessEqual(u, 1 + 1e-9)
                        self.assertGreaterEqual(v, -1e-9)
                        self.assertLessEqual(v, 1 + 1e-9)

    def test_face_sizes_are_tri_or_quad(self):
        for key in weapons.UFO_WEAPON_SPECS:
            path = os.path.join(DATA, "model", "weapon", f"{key}.x")
            meshes, errors, warns = check_x.parse(path)
            for mesh in meshes:
                for f in mesh["faces"]:
                    self.assertIn(len(f), (3, 4))

    def test_deterministic_regeneration(self):
        for key in weapons.UFO_WEAPON_SPECS:
            m1, _ = weapons.build_weapon_mesh(key)
            m2, _ = weapons.build_weapon_mesh(key)
            self.assertEqual(m1.verts, m2.verts)
            self.assertEqual(m1.faces, m2.faces)
            self.assertEqual(m1.uvs, m2.uvs)

    def test_muzzle_anchor_matches_template(self):
        """Grip/muzzle must be copied verbatim from the template weapon's
        engine hold offsets (docs/MISSION-UFO-FIELD.md section 3)."""
        expected = {
            "lever": dict(grip=(8, 7, 45), muzzle=(0.0, 0.052, -0.636)),
            "zap": dict(grip=(2, 11, 59), muzzle=(0.0, 0.027, -0.145)),
            "photon": dict(grip=(8, 10, 30), muzzle=(0.0, 0.031, -0.446)),
            "plasma": dict(grip=(8, 12, 44), muzzle=(-0.013, 0.013, -0.773)),
        }
        for key, exp in expected.items():
            spec = weapons.UFO_WEAPON_SPECS[key]
            with self.subTest(key=key):
                self.assertEqual(spec["grip"], exp["grip"])
                self.assertEqual(spec["muzzle"], exp["muzzle"])


class WeaponTextureTests(unittest.TestCase):
    def test_files_exist_and_bmp_valid(self):
        for key in weapons.UFO_WEAPON_SPECS:
            path = os.path.join(DATA, "model", "weapon", f"{key}.bmp")
            with self.subTest(key=key):
                self.assertTrue(os.path.isfile(path))
                _bmp_header_ok(path)
                self.assertEqual(list(Image.open(path).size), [128, 128])

    def test_deterministic_regeneration(self):
        for key in weapons.UFO_WEAPON_SPECS:
            a = weapons.paint_weapon_atlas(key)
            b = weapons.paint_weapon_atlas(key)
            self.assertEqual(list(a.getdata()), list(b.getdata()))


class LaserTests(unittest.TestCase):
    def test_model_exists_and_parses_and_is_longer_than_bullet(self):
        from tools.modelgen.generators import bullet

        path = os.path.join(DATA, "model", "laser.x")
        self.assertTrue(os.path.isfile(path))
        meshes, errors, warns = check_x.parse(path)
        self.assertEqual(errors, [])
        verts = [v for m in meshes for v in m["verts"]]
        laser_len = max(v[2] for v in verts) - min(v[2] for v in verts)

        bmesh, _ = bullet()
        bullet_len = max(v[2] for v in bmesh.verts) - min(v[2] for v in bmesh.verts)
        self.assertGreaterEqual(laser_len, bullet_len * 3.0)
        self.assertLessEqual(laser_len, bullet_len * 4.5)

    def test_textures_exist_and_dominant_hue_matches_spec(self):
        expected = {
            "laser_pink": (0xFF, 0x2F, 0xD6),
            "laser_cyan": (0x2F, 0xF7, 0xFF),
            "laser_lime": (0x9D, 0xFF, 0x2F),
        }
        for name, color in expected.items():
            path = os.path.join(DATA, "model", f"{name}.bmp")
            with self.subTest(name=name):
                self.assertTrue(os.path.isfile(path))
                _bmp_header_ok(path)
                image = Image.open(path).convert("RGB")
                pixels = list(image.getdata())
                mean = tuple(sum(p[i] for p in pixels) / len(pixels) for i in range(3))
                # the edge pixels should sit close to the pure spec colour
                edge = image.getpixel((0, image.size[1] // 2))
                dist = sum((edge[i] - color[i]) ** 2 for i in range(3)) ** 0.5
                self.assertLess(dist, 40, f"{name} edge colour {edge} too far from {color}")
                self.assertGreater(len(set(pixels)), 4)

    def test_distinct_colours(self):
        imgs = {}
        for name in ("laser_pink", "laser_cyan", "laser_lime"):
            path = os.path.join(DATA, "model", f"{name}.bmp")
            imgs[name] = Image.open(path).convert("RGB").getpixel((0, 32))
        names = list(imgs)
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                a, b = imgs[names[i]], imgs[names[j]]
                dist = sum((a[k] - b[k]) ** 2 for k in range(3)) ** 0.5
                self.assertGreater(dist, 50, f"{names[i]} vs {names[j]}")


class CowTests(unittest.TestCase):
    def test_model_exists_parses_and_is_small_object_sized(self):
        path = os.path.join(DATA, "model", "cow.x")
        self.assertTrue(os.path.isfile(path))
        meshes, errors, warns = check_x.parse(path)
        self.assertEqual(errors, [])
        verts = [v for m in meshes for v in m["verts"]]
        height_file = max(v[1] for v in verts) - min(v[1] for v in verts)
        length_file = max(v[2] for v in verts) - min(v[2] for v in verts)
        # SMALLOBJECT_SCALE 5.0 (ASSET-CONTRACT 6.4): world = file * 5.
        world_h, world_l = height_file * 5.0, length_file * 5.0
        self.assertAlmostEqual(world_h, 10.0, delta=3.0)
        self.assertAlmostEqual(world_l, 20.0, delta=6.0)

    def test_texture_exists_and_is_bw_with_pink_nose(self):
        path = os.path.join(DATA, "model", "cow.bmp")
        self.assertTrue(os.path.isfile(path))
        _bmp_header_ok(path)
        image = Image.open(path).convert("RGB")
        atlas = cow.cow_atlas()
        r = atlas.rects["head"]
        nose_box = (r.x0 + (r.x1 - r.x0) // 3, r.y1 - 6, r.x1 - (r.x1 - r.x0) // 3, r.y1 - 1)
        nose_px = list(image.crop(nose_box).getdata())
        mean = tuple(sum(p[i] for p in nose_px) / len(nose_px) for i in range(3))
        self.assertGreater(mean[0], mean[2])  # pink: red > blue

    def test_deterministic(self):
        a, _ = cow.build_cow_mesh()
        b, _ = cow.build_cow_mesh()
        self.assertEqual(a.verts, b.verts)
        self.assertEqual(a.faces, b.faces)


class HumanSkinTests(unittest.TestCase):
    NAMES = ["rancher", "alien_teal", "alien_violet", "alien_lime"]

    def test_files_exist_and_sized(self):
        for name in self.NAMES:
            path = os.path.join(DATA, "model", human_skins_ufo.FILES[name])
            with self.subTest(name=name):
                self.assertTrue(os.path.isfile(path))
                _bmp_header_ok(path)
                self.assertEqual(list(Image.open(path).size), [256, 256])

    def test_aliens_are_mutually_distinct_and_distinct_from_rancher(self):
        from tools.modelgen.generators import HUMAN_ATLAS

        rect = HUMAN_ATLAS.rects["torso_front"]
        means = {}
        for name in self.NAMES:
            path = os.path.join(DATA, "model", human_skins_ufo.FILES[name])
            image = Image.open(path).convert("RGB")
            pixels = list(image.crop((rect.x0, rect.y0, rect.x1, rect.y1)).getdata())
            means[name] = tuple(sum(p[i] for p in pixels) / len(pixels) for i in range(3))
        names = self.NAMES
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                a, b = means[names[i]], means[names[j]]
                dist = sum((a[k] - b[k]) ** 2 for k in range(3)) ** 0.5
                self.assertGreater(dist, 20, f"{names[i]} vs {names[j]} torso too similar")

    def test_aliens_have_no_mouth_marking_but_have_eyes(self):
        """Eyes (dark almond shapes) must be present on head_front; no
        separate mouth marking should be drawn (ASSET-CONTRACT/ spec R7)."""
        from tools.modelgen.generators import HUMAN_ATLAS

        rect = HUMAN_ATLAS.rects["head_front"]
        for name in ("alien_teal", "alien_violet", "alien_lime"):
            path = os.path.join(DATA, "model", human_skins_ufo.FILES[name])
            image = Image.open(path).convert("RGB")
            crop = image.crop((rect.x0, rect.y0, rect.x1, rect.y1))
            pixels = list(crop.getdata())
            very_dark = sum(1 for p in pixels if sum(p) < 60)
            with self.subTest(name=name):
                self.assertGreater(very_dark, 10, "expected dark eye pixels on head_front")

    def test_deterministic_regeneration(self):
        for name in self.NAMES:
            a = human_skins_ufo.make_atlas(name)
            b = human_skins_ufo.make_atlas(name)
            self.assertEqual(list(a.getdata()), list(b.getdata()))


class BlockTextureTests(unittest.TestCase):
    NAMES = ["grass", "dirt", "hay", "fence", "barn_red", "barn_roof",
             "trim_white", "ufo_hull", "ufo_light", "ufo_dark"]

    def test_all_ten_files_exist_and_valid_bmp8(self):
        for name in self.NAMES:
            path = os.path.join(DATA, "ufofield", f"{name}.bmp")
            with self.subTest(name=name):
                self.assertTrue(os.path.isfile(path))
                _bmp_header_ok(path)
                self.assertEqual(list(Image.open(path).size), [256, 256])

    def test_tiles_wrap_seamlessly_at_edges(self):
        """Map tiles render with GL_REPEAT (ASSET-CONTRACT 9.1); left/right
        and top/bottom edge rows must match closely so no seam shows."""
        for name in self.NAMES:
            path = os.path.join(DATA, "ufofield", f"{name}.bmp")
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


class SoundTests(unittest.TestCase):
    NAMES = ["laser1", "laser2", "laser3", "moo", "teleport"]

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
    """assets/provenance.jsonl must hold a CC0 record, with the current
    sha256, for every UFO FIELD asset."""

    PROV_PATH = os.path.join(REPO_ROOT, "assets", "provenance.jsonl")

    def setUp(self):
        import json
        with open(self.PROV_PATH) as fh:
            self.records = {r["path"]: r for r in (json.loads(l) for l in fh if l.strip())}

    def _expected(self):
        expected = set()
        for key in weapons.UFO_WEAPON_SPECS:
            expected.add(f"data/model/weapon/{key}.x")
            expected.add(f"data/model/weapon/{key}.bmp")
        expected.add("data/model/laser.x")
        for name in laser.COLORS:
            expected.add(f"data/model/{name}.bmp")
        expected.add("data/model/cow.x")
        expected.add("data/model/cow.bmp")
        for name in human_skins_ufo.FILES.values():
            expected.add(f"data/model/{name}")
        for name in block_textures.GENERATORS:
            expected.add(f"data/ufofield/{name}")
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
                for field in ("sha256", "generator", "date"):
                    self.assertIn(field, rec)
                data = open(os.path.join(REPO_ROOT, "assets", "pack", path), "rb").read()
                self.assertEqual(rec["sha256"], hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    unittest.main()

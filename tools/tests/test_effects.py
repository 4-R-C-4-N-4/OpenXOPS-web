"""Tests for tools/artgen/effects.py: data/{blood,mflash,smoke,yakkyou}.dds.

Checks against docs/ASSET-CONTRACT.md section 3.1 (effect billboards) and
assets/manifest.json slot fields (128x128, dds_argb8, alpha "channel").
"""
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "artgen"))

import dds  # noqa: E402
import effects  # noqa: E402

SIZE = 128
BORDER_PTS = [(0, 0), (SIZE - 1, 0), (0, SIZE - 1), (SIZE - 1, SIZE - 1),
              (SIZE // 2, 0), (0, SIZE // 2), (SIZE - 1, SIZE // 2), (SIZE // 2, SIZE - 1)]
CENTER_PT = (SIZE // 2, SIZE // 2)


class EffectsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="effects_test_")
        cls.images = {path: gen() for path, gen in effects.GENERATORS.items()}
        cls.paths = {}
        for path, im in cls.images.items():
            dst = os.path.join(cls.tmpdir, Path(path).name)
            dds.write_dds(im, dst, fmt="dds_argb8")
            cls.paths[path] = dst

    def test_all_four_slots_present(self):
        self.assertEqual(set(effects.GENERATORS),
                          {"data/blood.dds", "data/mflash.dds",
                           "data/smoke.dds", "data/yakkyou.dds"})

    def test_header_format_size_and_engine_decode(self):
        for path, dst in self.paths.items():
            with self.subTest(path=path):
                data = Path(dst).read_bytes()
                self.assertEqual(data[:4], b"DDS ")
                probs = dds.verify_dds(dst, (SIZE, SIZE), "dds_argb8")
                self.assertEqual(probs, [], probs)
                info = dds.info(data)
                self.assertEqual(info["width"], SIZE)
                self.assertEqual(info["height"], SIZE)
                self.assertEqual(info["bits"], 32)
                self.assertEqual(len(data), 128 + SIZE * SIZE * 4)
                w, h, px = dds.engine_load(dst)
                self.assertEqual((w, h), (SIZE, SIZE))

    def test_border_alpha_zero(self):
        for path, dst in self.paths.items():
            with self.subTest(path=path):
                w, h, px = dds.engine_load(dst)
                for x, y in BORDER_PTS:
                    a = px[4 * (y * w + x) + 3]
                    self.assertEqual(a, 0, "%s border (%d,%d) alpha=%d" % (path, x, y, a))

    def test_centre_alpha_high(self):
        for path, dst in self.paths.items():
            with self.subTest(path=path):
                w, h, px = dds.engine_load(dst)
                x, y = CENTER_PT
                a = px[4 * (y * w + x) + 3]
                self.assertGreater(a, 180, "%s centre alpha too low: %d" % (path, a))

    def test_determinism(self):
        for path, gen in effects.GENERATORS.items():
            with self.subTest(path=path):
                im1 = gen()
                im2 = gen()
                self.assertEqual(im1.tobytes(), im2.tobytes())

    def test_byte_identical_dds_regeneration(self):
        for path, gen in effects.GENERATORS.items():
            with self.subTest(path=path):
                im = gen()
                p1 = os.path.join(self.tmpdir, "regen1_" + Path(path).name)
                p2 = os.path.join(self.tmpdir, "regen2_" + Path(path).name)
                dds.write_dds(im, p1, fmt="dds_argb8")
                dds.write_dds(gen(), p2, fmt="dds_argb8")
                self.assertEqual(Path(p1).read_bytes(), Path(p2).read_bytes())

    def test_images_are_rgba_at_expected_size(self):
        for path, im in self.images.items():
            with self.subTest(path=path):
                self.assertEqual(im.size, (SIZE, SIZE))
                self.assertEqual(im.mode, "RGBA")


if __name__ == "__main__":
    unittest.main()

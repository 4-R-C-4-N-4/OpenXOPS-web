"""Tests for tools/artgen/char_atlas.py: data/char.dds / data/title.dds.

Runs the generator once (network-cached Shinonome tarball, same pin as
tools/fonts/gen_font_atlas.py) and checks the output against
docs/ASSET-CONTRACT.md section 2 / tools/contract/char_layout.json.
"""
import json
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "artgen"))

import dds  # noqa: E402
import char_atlas  # noqa: E402


class CharAtlasTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="char_atlas_test_")
        cls.char_path = os.path.join(cls.tmpdir, "char.dds")
        cls.title_path = os.path.join(cls.tmpdir, "title.dds")
        atlas = char_atlas.build_char_atlas()
        dds.write_dds(atlas, cls.char_path, fmt="dds_argb4")
        cls.atlas = atlas
        logo = char_atlas.build_title_logo()
        dds.write_dds(logo, cls.title_path, fmt="dds_argb8")
        cls.logo = logo

    # --- char.dds ---------------------------------------------------

    def test_char_header_format_size(self):
        data = Path(self.char_path).read_bytes()
        self.assertEqual(data[:4], b"DDS ")
        probs = dds.verify_dds(self.char_path, (512, 512), "dds_argb4")
        self.assertEqual(probs, [], probs)
        info = dds.info(data)
        self.assertEqual(info["width"], 512)
        self.assertEqual(info["height"], 512)
        self.assertEqual(info["bits"], 16)
        self.assertEqual(len(data), 128 + 512 * 512 * 2)

    def test_determinism(self):
        atlas2 = char_atlas.build_char_atlas()
        self.assertEqual(atlas2.tobytes(), self.atlas.tobytes())
        p2 = os.path.join(self.tmpdir, "char2.dds")
        dds.write_dds(atlas2, p2, fmt="dds_argb4")
        self.assertEqual(Path(self.char_path).read_bytes(), Path(p2).read_bytes())

    def test_glyph_cell_A_nonempty(self):
        w, h, px = dds.engine_load(self.char_path)
        code = ord("A")
        col, row = code % 16, code // 16
        x0, y0 = col * 32, row * 32
        found_alpha = False
        for y in range(y0, y0 + 32):
            for x in range(x0, x0 + 32):
                k = (y * w + x) * 4
                if px[k + 3] > 0:
                    found_alpha = True
                    break
            if found_alpha:
                break
        self.assertTrue(found_alpha, "'A' cell has no visible (alpha>0) pixels")

    def test_space_cell_empty(self):
        w, h, px = dds.engine_load(self.char_path)
        code = ord(" ")
        col, row = code % 16, code // 16
        x0, y0 = col * 32, row * 32
        for y in range(y0, y0 + 32):
            for x in range(x0, x0 + 32):
                k = (y * w + x) * 4
                self.assertEqual(px[k + 3], 0, "space (0x20) cell must be fully transparent")

    def test_hud_cells_from_contract_are_nonempty(self):
        layout = json.loads((ROOT / "tools" / "contract" / "char_layout.json").read_text())
        w, h, px = dds.engine_load(self.char_path)
        for cell in layout["hud_cells"]:
            x0, y0, x1, y1 = cell["rect_px"]
            found = False
            for y in range(y0, y1):
                for x in range(x0, x1):
                    k = (y * w + x) * 4
                    if px[k + 3] > 0:
                        found = True
                        break
                if found:
                    break
            self.assertTrue(found, f"HUD cell {cell['code']} ({cell['purpose']}) is empty")

    def test_grid_matches_contract(self):
        layout = json.loads((ROOT / "tools" / "contract" / "char_layout.json").read_text())
        self.assertEqual(layout["grid"]["cols"], 16)
        self.assertEqual(layout["grid"]["rows"], 16)
        self.assertEqual(layout["grid"]["cell_px_at_512"], 32)

    # --- title.dds ----------------------------------------------------

    def test_title_header_format_size(self):
        probs = dds.verify_dds(self.title_path, (480, 80), "dds_argb8")
        self.assertEqual(probs, [], probs)
        data = Path(self.title_path).read_bytes()
        self.assertEqual(len(data), 128 + 480 * 80 * 4)

    def test_title_nonempty(self):
        w, h, px = dds.engine_load(self.title_path)
        alphas = px[3::4]
        self.assertGreater(max(alphas), 0)


if __name__ == "__main__":
    unittest.main()

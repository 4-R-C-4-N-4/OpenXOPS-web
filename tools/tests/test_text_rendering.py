"""CP932 decode + Shinonome bitmap text rendering tests.

Compiles tests/text/cp932_harness.cpp against src/platform/text with a
plain host g++ (no Emscripten/SDL/GL dependency) and asserts on its output.
"""
import os
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
HARNESS = os.path.join(ROOT, "tests", "text", "cp932_harness.cpp")
TEXT_CPP = os.path.join(ROOT, "src", "platform", "text", "text.cpp")


class TestTextRendering(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="cp932_harness_")
        exe = os.path.join(cls.tmpdir, "cp932_harness")
        subprocess.run(
            ["g++", "-std=c++17", "-O0", "-o", exe, HARNESS, TEXT_CPP],
            check=True, cwd=ROOT,
        )
        out = subprocess.run([exe], check=True, capture_output=True, text=True, cwd=ROOT)
        cls.values = {}
        for line in out.stdout.splitlines():
            for pair in line.split():
                k, v = pair.split("=", 1)
                cls.values[k] = v

    def test_ascii_passthrough(self):
        self.assertEqual(self.values["ascii_ok"], "1")
        self.assertEqual(self.values["ascii_cp"], "65")  # 'A'
        self.assertEqual(self.values["ascii_used"], "1")

    def test_known_cp932_double_byte(self):
        self.assertEqual(self.values["hiragana_a_cp"], "0x3042")  # あ
        self.assertEqual(self.values["hiragana_a_used"], "2")
        self.assertEqual(self.values["hiragana_i_cp"], "0x3044")  # い
        self.assertEqual(self.values["hiragana_i_used"], "2")
        self.assertEqual(self.values["kanji_hi_cp"], "0x65e5")  # 日
        self.assertEqual(self.values["kanji_hi_used"], "2")

    def test_truncated_lead_byte_is_safe(self):
        self.assertEqual(self.values["truncated_ok"], "0")
        self.assertEqual(self.values["truncated_used"], "1")  # never past remaining buffer
        self.assertEqual(self.values["truncated_cp"], "0xfffd")

    def test_malformed_trail_byte_resyncs(self):
        self.assertEqual(self.values["malformed_ok"], "0")
        self.assertEqual(self.values["malformed_used"], "1")  # only the lead byte

    def test_crlf_carriage_return_is_invisible(self):
        self.assertEqual(self.values["crlf_w"], "16")  # "AB" only
        self.assertEqual(self.values["crlf_h"], "32")

    def test_empty_input(self):
        self.assertEqual(self.values["empty_ok"], "0")
        self.assertEqual(self.values["empty_used"], "0")

    def test_measure_half_width(self):
        self.assertEqual(self.values["measure_half_w"], "24")  # 3 * (16/2)
        self.assertEqual(self.values["measure_half_h"], "16")

    def test_measure_full_width(self):
        self.assertEqual(self.values["measure_full_w"], "32")  # 2 * 16
        self.assertEqual(self.values["measure_full_h"], "16")

    def test_measure_mixed_width_and_lines(self):
        # Line 1: 'A' (8px) + full-width glyph (16px) = 24px; line 2: "BC" = 16px.
        self.assertEqual(self.values["measure_mixed_w"], "24")
        self.assertEqual(self.values["measure_mixed_h"], "32")

    def test_bitmap_sane_and_nonblank(self):
        self.assertEqual(self.values["bitmap_ok"], "1")
        self.assertEqual(self.values["bitmap_w"], "8")
        self.assertEqual(self.values["bitmap_h"], "16")
        self.assertEqual(self.values["bitmap_size"], str(8 * 16 * 4))
        self.assertGreater(int(self.values["bitmap_opaque"]), 0)

    def test_real_kana_kanji_glyphs_not_outline_boxes(self):
        # Real Shinonome glyph data must paint interior pixels, not just the
        # 1px outline-box placeholder border, for hiragana, katakana, a
        # multi-kanji string, a JIS level-2 kanji, and full-width '!'.
        for name in (
            "glyph_hiragana_a", "glyph_katakana_a", "glyph_nihon",
            "glyph_level2_kanji", "glyph_fullwidth_bang",
        ):
            with self.subTest(name=name):
                self.assertGreater(int(self.values[f"{name}_opaque"]), 0)
                self.assertGreater(int(self.values[f"{name}_interior"]), 0)

    def test_invalid_arguments_rejected(self):
        self.assertEqual(self.values["invalid_null"], "0")
        self.assertEqual(self.values["invalid_px"], "0")


if __name__ == "__main__":
    unittest.main()

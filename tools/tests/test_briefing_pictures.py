"""Tests for the procedural briefing picture (data/briefing/starter.bmp) and
the start.txt briefing text constraints (on-screen text box limits)."""

import statistics
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from PIL import Image  # noqa: E402

from tools.artgen import briefing_pictures  # noqa: E402

BMP_PATH = ROOT / "assets" / "pack" / "data" / "briefing" / "starter.bmp"
START_TXT = ROOT / "assets" / "pack" / "data" / "starter" / "start.txt"

# Engine text-box constraints for the briefing screen: body text starts at
# y=180 on a 480-high screen, 16px per line, with the "LEFT CLICK TO BEGIN"
# prompt around y~430, so at most 14 lines fit; each glyph is 8px wide and
# the text column runs from x=230 to the 640 edge, so at most 52 chars/line.
MAX_BODY_LINES = 14
MAX_LINE_CHARS = 52


class BriefingPictureTests(unittest.TestCase):
    def test_file_exists(self):
        self.assertTrue(BMP_PATH.exists(), f"missing {BMP_PATH}")

    def test_is_valid_8bit_bmp_of_expected_size(self):
        with open(BMP_PATH, "rb") as f:
            header = f.read(14 + 40)
        magic, size, _, _, offbits = struct.unpack_from("<2sIHHI", header, 0)
        self.assertEqual(magic, b"BM")
        (hdr_size, width, height, planes, bitcount, compression,
         img_size, _xppm, _yppm, clr_used, _clr_important) = \
            struct.unpack_from("<IiiHHIIiiII", header, 14)
        self.assertEqual(hdr_size, 40)
        self.assertEqual(bitcount, 8)
        self.assertEqual(compression, 0)
        self.assertGreater(height, 0, "height must be positive (top-down "
                                       "is not supported by the engine)")
        self.assertEqual((width, height), (briefing_pictures.SIZE,
                                            briefing_pictures.SIZE))
        self.assertEqual(offbits, 14 + 40 + 1024)

        img = Image.open(BMP_PATH)
        self.assertEqual(img.mode, "P")
        self.assertEqual(img.size, (briefing_pictures.SIZE,
                                     briefing_pictures.SIZE))

    def test_not_mostly_white(self):
        img = Image.open(BMP_PATH).convert("L")
        mean = statistics.mean(img.getdata())
        self.assertLess(mean, 200.0, "picture looks blank/white")

    def test_deterministic_regeneration(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            briefing_pictures.generate_starter(out_dir)
            regenerated = (out_dir / "starter.bmp").read_bytes()
        committed = BMP_PATH.read_bytes()
        self.assertEqual(regenerated, committed,
                          "regeneration is not byte-for-byte deterministic")

    def test_start_txt_names_starter_picture(self):
        lines = START_TXT.read_text(encoding="ascii").split("\n")
        self.assertEqual(lines[0], "starter")
        self.assertEqual(lines[1], "!")


class StartTxtLayoutTests(unittest.TestCase):
    """Briefing body text must fit on-screen without overlapping the
    'LEFT CLICK TO BEGIN' prompt, and must stay ASCII."""

    def _body_lines(self):
        raw = START_TXT.read_bytes()
        raw.decode("ascii")  # must be pure ASCII
        lines = raw.decode("ascii").split("\n")
        # lines[0:3] are picture A, picture B, sky index; body follows.
        return lines[3:]

    def test_body_line_count_within_box(self):
        body = self._body_lines()
        self.assertLessEqual(
            len(body), MAX_BODY_LINES,
            f"briefing body has {len(body)} lines, must be <= "
            f"{MAX_BODY_LINES} to avoid overlapping the prompt text",
        )

    def test_body_line_width_within_box(self):
        body = self._body_lines()
        for i, line in enumerate(body):
            self.assertLessEqual(
                len(line), MAX_LINE_CHARS,
                f"body line {i} ({line!r}) is {len(line)} chars, must be "
                f"<= {MAX_LINE_CHARS}",
            )

    def test_body_is_ascii(self):
        START_TXT.read_bytes().decode("ascii")

    def test_no_stale_south_gate_reference(self):
        """The south gate is now a closed wall; the briefing text must not
        claim the player enters through it."""
        text = START_TXT.read_text(encoding="ascii").lower()
        self.assertNotIn("through the south gate", text)


if __name__ == "__main__":
    unittest.main()

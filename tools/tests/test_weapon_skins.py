"""Tests for tools/artgen/weapon_skins.py: the 7 P0 weapon/bullet/human
textures that make free-pack weapons and the HUD icon stop rendering as
checkerboards."""
import struct
import unittest
from pathlib import Path

from PIL import Image

from tools.artgen.weapon_skins import (
    OUT_MODEL,
    OUT_WEAPON,
    WEAPON_FILES,
    paint_bullet_atlas,
    paint_soldier0_atlas,
    paint_weapon_atlas,
    save_bmp8,
)
from tools.modelgen.generators import HUMAN_ATLAS, _weapon_atlas, bullet

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets/manifest.json"

FILES = {
    "m92f": (OUT_WEAPON / "m92f.bmp", "weapon"),
    "de": (OUT_WEAPON / "de.bmp", "weapon"),
    "mac10": (OUT_WEAPON / "mac10.bmp", "weapon"),
    "m4": (OUT_WEAPON / "m4.bmp", "weapon"),
    "grenade": (OUT_WEAPON / "grenade.bmp", "weapon"),
    "bullet": (OUT_MODEL / "bullet.bmp", "bullet"),
    "soldier0": (OUT_MODEL / "soldier0.bmp", "human"),
}


def _manifest_slot(path: str) -> dict:
    import json

    manifest = json.loads(MANIFEST.read_text())
    by_path = {slot["path"]: slot for slot in manifest["slots"]}
    return by_path[path]


def _rel_path(p: Path) -> str:
    return str(p.relative_to(ROOT / "assets/pack"))


class WeaponSkinTests(unittest.TestCase):
    def test_all_seven_files_exist(self):
        for key, (path, _) in FILES.items():
            with self.subTest(key=key):
                self.assertTrue(path.is_file(), path)

    def test_bmp_header_is_uncompressed_8bit_with_full_palette(self):
        for key, (path, _) in FILES.items():
            with self.subTest(key=key):
                raw = path.read_bytes()
                self.assertEqual(raw[:2], b"BM")
                biSize, width, height, planes, bpp, compression = struct.unpack_from(
                    "<IiiHHI", raw, 14
                )
                self.assertEqual(biSize, 40)
                self.assertGreater(height, 0)
                self.assertEqual(bpp, 8)
                self.assertEqual(compression, 0)
                off_bits = struct.unpack_from("<I", raw, 10)[0]
                self.assertEqual(off_bits, 14 + 40 + 1024)

    def test_sizes_match_manifest_ref_size(self):
        for key, (path, _) in FILES.items():
            with self.subTest(key=key):
                slot_path = _rel_path(path)
                slot = _manifest_slot(slot_path)
                self.assertEqual(slot["ref_format"], "bmp8")
                image = Image.open(path)
                self.assertEqual(list(image.size), slot["ref_size"])

    def test_weapon_atlas_rects_are_not_flat_or_checker(self):
        atlas = _weapon_atlas()
        for key in ("m92f", "de", "mac10", "m4", "grenade"):
            path, _ = FILES[key]
            image = Image.open(path).convert("RGB")
            for name in ("body", "barrel", "grip", "misc"):
                rect = atlas.rects[name]
                crop = image.crop((rect.x0, rect.y0, rect.x1, rect.y1))
                with self.subTest(key=key, rect=name):
                    self.assertGreaterEqual(len(set(crop.getdata())), 4)

    def test_bullet_atlas_rect_is_not_flat(self):
        _, atlas = bullet()
        image = Image.open(FILES["bullet"][0]).convert("RGB")
        rect = atlas.rects["skin"]
        crop = image.crop((rect.x0, rect.y0, rect.x1, rect.y1))
        self.assertGreaterEqual(len(set(crop.getdata())), 4)
        # brass-ish mean: red channel clearly above blue
        pixels = list(crop.getdata())
        mean_r = sum(p[0] for p in pixels) / len(pixels)
        mean_b = sum(p[2] for p in pixels) / len(pixels)
        self.assertGreater(mean_r, mean_b)

    def test_soldier0_human_atlas_rects_are_not_flat(self):
        image = Image.open(FILES["soldier0"][0]).convert("RGB")
        for name in ("head_front", "torso_front", "leg_upper", "foot"):
            rect = HUMAN_ATLAS.rects[name]
            crop = image.crop((rect.x0, rect.y0, rect.x1, rect.y1))
            with self.subTest(rect=name):
                self.assertGreaterEqual(len(set(crop.getdata())), 4)

    def test_soldier0_is_distinct_from_green_and_black_variants(self):
        tan = Image.open(FILES["soldier0"][0]).convert("RGB")
        for other_name in ("soldier_green.bmp", "soldier_black.bmp"):
            other_path = OUT_MODEL / other_name
            if not other_path.is_file():
                continue
            other = Image.open(other_path).convert("RGB")
            rect = HUMAN_ATLAS.rects["torso_front"]
            tan_pixels = list(tan.crop((rect.x0, rect.y0, rect.x1, rect.y1)).getdata())
            other_pixels = list(other.crop((rect.x0, rect.y0, rect.x1, rect.y1)).getdata())
            tan_mean = tuple(sum(p[i] for p in tan_pixels) / len(tan_pixels) for i in range(3))
            other_mean = tuple(sum(p[i] for p in other_pixels) / len(other_pixels) for i in range(3))
            dist = sum((tan_mean[i] - other_mean[i]) ** 2 for i in range(3)) ** 0.5
            with self.subTest(other=other_name):
                self.assertGreater(dist, 15)

    def test_regeneration_is_byte_identical(self):
        for key, filename in WEAPON_FILES.items():
            a = paint_weapon_atlas(key)
            b = paint_weapon_atlas(key)
            self.assertEqual(list(a.getdata()), list(b.getdata()))

        a1, b1 = paint_bullet_atlas(), paint_bullet_atlas()
        self.assertEqual(list(a1.getdata()), list(b1.getdata()))

        a2, b2 = paint_soldier0_atlas(), paint_soldier0_atlas()
        self.assertEqual(list(a2.getdata()), list(b2.getdata()))

    def test_saved_bmps_are_byte_identical_across_runs(self):
        import tempfile

        for key, (path, kind) in FILES.items():
            with tempfile.TemporaryDirectory(prefix="weapon_skin_bmp_") as temp:
                out = Path(temp) / "out.bmp"
                if kind == "weapon":
                    image = paint_weapon_atlas(key)
                elif kind == "bullet":
                    image = paint_bullet_atlas()
                else:
                    image = paint_soldier0_atlas()
                save_bmp8(image, out)
                with self.subTest(key=key):
                    self.assertEqual(out.read_bytes(), path.read_bytes())


if __name__ == "__main__":
    unittest.main()

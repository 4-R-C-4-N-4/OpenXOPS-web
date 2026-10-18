import hashlib
import json
import re
import struct
import unittest
from pathlib import Path

from PIL import Image

from tools.artgen.human_skins import OUT_DIR, SOURCE, make_atlas
from tools.modelgen.generators import HUMAN_ATLAS

ROOT = Path(__file__).resolve().parents[2]


class HumanSkinTests(unittest.TestCase):
    def test_active_free_pack_human_texture_paths_are_real(self):
        active = set()
        for example in (ROOT / "tools/mapgen/examples/starter_compound.json",
                        ROOT / "tools/mapgen/examples/map10_demo.json"):
            doc = json.loads(example.read_text())
            active.update(point["human_type"] for point in doc["points"]
                          if point["type"] in ("human", "player_spawn"))
        source = (ROOT / "src/parameter.cpp").read_text()
        table = {int(i): path for i, path in re.findall(
            r'HumanTexturePath\[(\d+)\]\s*=\s*"\./data/model/([^\"]+)"', source)}
        assignments = {int(i): int(tex) for i, tex in re.findall(
            r'Human\[(\d+)\]\.texture\s*=\s*(\d+)', source)}
        manifest = json.loads((ROOT / "assets/manifest.json").read_text())
        by_path = {slot["path"]: slot for slot in manifest["slots"]}
        paths = set()
        for human_type in active:
            path = "data/model/" + table[assignments[human_type]]
            paths.add(path)
            self.assertTrue((ROOT / "assets/pack" / path).is_file(), path)
            self.assertNotEqual(by_path[path]["status"], "missing", path)
        self.assertEqual(paths, {"data/model/soldier_green.bmp", "data/model/soldier_black.bmp"})

    def test_bmp_is_uncompressed_opaque_atlas_with_visible_face(self):
        for filename in ("soldier_green.bmp", "soldier_black.bmp"):
            raw = (OUT_DIR / filename).read_bytes()
            self.assertEqual(len(raw), 66614)
            self.assertEqual(raw[:2], b"BM")
            self.assertEqual(struct.unpack_from("<I", raw, 30)[0], 0)
            image = Image.open(OUT_DIR / filename).convert("RGB")
            self.assertEqual(image.size, (256, 256))
            face = HUMAN_ATLAS.rects["head_front"]
            crop = image.crop((face.x0, face.y0, face.x1, face.y1))
            self.assertGreater(len(set(crop.getdata())), 4)
            self.assertNotEqual(crop.getpixel((9, 13)), crop.getpixel((16, 22)))

    def test_deterministic_from_retained_image_source(self):
        image = Image.open(SOURCE).convert("RGB")
        first, second = make_atlas(image, "green"), make_atlas(image, "green")
        self.assertEqual(hashlib.sha256(first.tobytes()).digest(),
                         hashlib.sha256(second.tobytes()).digest())
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                         "54cb5c2091f23a416a7f238bb8be9805dcb496cde18f122bf5532954d0387ab9")


if __name__ == "__main__":
    unittest.main()

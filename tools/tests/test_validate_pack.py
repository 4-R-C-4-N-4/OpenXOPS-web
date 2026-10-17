"""Unit tests for tools/validate_pack.py (stdlib unittest, synthetic fixtures only)."""
import hashlib
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..")))
import validate_pack as vp  # noqa: E402


def bmp8(w=4, h=4, compression=0):
    """A tiny 8-bit BMP. compression=1 writes a valid RLE8 stream."""
    palette = b"".join(struct.pack("<BBBB", i, i, i, 0) for i in range(256))
    if compression == 0:
        stride = (w + 3) & ~3
        pixels = b"".join(bytes([(x + y) % 256 for x in range(w)]) + b"\0" * (stride - w) for y in range(h))
    else:
        rows = [bytes([w, (y * 7) % 256]) + b"\x00\x00" for y in range(h)]
        pixels = b"".join(rows)[:-2] + b"\x00\x01"           # last EOL -> end of bitmap
    off = 14 + 40 + len(palette)
    header = b"BM" + struct.pack("<IHHI", off + len(pixels), 0, 0, off)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, compression, len(pixels), 2835, 2835, 256, 0)
    return header + info + palette + pixels


def wav(tag=1, ch=1, rate=22050, bits=16, frames=8):
    data = b"\0" * (frames * ch * bits // 8)
    fmt = struct.pack("<HHIIHH", tag, ch, rate, rate * ch * bits // 8, ch * bits // 8, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


def slot(path, kind="texture", cls="skin", prio="P0", size=(4, 4), fmt="bmp8"):
    return {"path": path, "kind": kind, "class": cls, "hard": True, "source_refs": [],
            "ref_size": list(size) if size else None, "ref_format": fmt, "alpha": "none",
            "tileable": False, "uv_layout": None, "summary": "test", "priority": prio,
            "status": "missing", "note": None}


class PackFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="vp-test-")
        self.pack = os.path.join(self.tmp, "pack")
        os.makedirs(self.pack)
        self.manifest = os.path.join(self.tmp, "manifest.json")
        self.reference = os.path.join(self.tmp, "reference.json")
        self.provenance = os.path.join(self.tmp, "provenance.jsonl")
        self.write_manifest([slot("data/model/soldier0.bmp")])
        self.write_reference([])
        open(self.provenance, "w").close()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    # -- helpers --------------------------------------------------------------
    def write_manifest(self, slots):
        with open(self.manifest, "w") as f:
            json.dump({"version": 1, "slots": slots}, f)

    def write_reference(self, md5s):
        files = [{"path": "data/orig%d.bmp" % i, "md5": m, "size": 1, "width": None, "height": None,
                  "format": None} for i, m in enumerate(md5s)]
        with open(self.reference, "w") as f:
            json.dump({"files": files}, f)

    def put(self, rel, data, provenance=True):
        full = os.path.join(self.pack, *rel.split("/"))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)
        if provenance:
            self.add_provenance(rel, hashlib.sha256(data).hexdigest())

    def add_provenance(self, rel, sha):
        rec = {"path": rel, "sha256": sha, "generator": "unit-test", "prompt": "synthetic fixture",
               "license": "CC0-1.0", "author": "tests", "date": "2026-10-17", "curated_by": None}
        with open(self.provenance, "a") as f:
            f.write(json.dumps(rec) + "\n")

    def run_validate(self, strict=False):
        rep, _ = vp.validate(self.pack, self.manifest, self.reference, self.provenance, strict)
        return rep

    def checks(self, rep, level="FAIL"):
        return {(c, p) for lv, c, p, _m in rep.issues if lv == level}


class TestBmp(PackFixture):
    def test_rle8_bmp_rejected(self):
        self.put("data/model/soldier0.bmp", bmp8(compression=1))
        rep = self.run_validate()
        self.assertIn(("d", "data/model/soldier0.bmp"), self.checks(rep))

    def test_uncompressed_bmp8_passes(self):
        self.put("data/model/soldier0.bmp", bmp8())
        rep = self.run_validate()
        self.assertEqual(rep.failures, [])
        with redirect_stdout(io.StringIO()):
            code = vp.main([self.pack, "--manifest", self.manifest, "--reference", self.reference,
                            "--provenance", self.provenance])
        self.assertEqual(code, 0)

    def test_size_mismatch_warns_then_fails_strict(self):
        self.put("data/model/soldier0.bmp", bmp8(8, 8))
        self.assertIn(("c", "data/model/soldier0.bmp"), self.checks(self.run_validate(), "WARN"))
        self.assertIn(("c", "data/model/soldier0.bmp"), self.checks(self.run_validate(strict=True)))


class TestCleanRoom(PackFixture):
    def test_reference_md5_rejected(self):
        data = bmp8()
        self.put("data/model/soldier0.bmp", data)
        self.write_reference(["0" * 32, hashlib.md5(data).hexdigest()])
        rep = self.run_validate()
        self.assertIn(("f", "data/model/soldier0.bmp"), self.checks(rep))

    def test_unrelated_md5_passes(self):
        self.put("data/model/soldier0.bmp", bmp8())
        self.write_reference(["0" * 32])
        self.assertEqual(self.run_validate().failures, [])


class TestProvenance(PackFixture):
    def test_missing_provenance_rejected(self):
        self.put("data/model/soldier0.bmp", bmp8(), provenance=False)
        rep = self.run_validate()
        self.assertIn(("g", "data/model/soldier0.bmp"), self.checks(rep))

    def test_sha256_mismatch_rejected(self):
        self.put("data/model/soldier0.bmp", bmp8(), provenance=False)
        self.add_provenance("data/model/soldier0.bmp", "ab" * 32)
        self.assertIn(("g", "data/model/soldier0.bmp"), self.checks(self.run_validate()))

    def test_unlisted_file_still_needs_provenance(self):
        self.put("data/model/soldier0.bmp", bmp8())
        self.put("data/extra.txt", b"hello", provenance=False)
        rep = self.run_validate()
        self.assertIn(("g", "data/extra.txt"), self.checks(rep))
        self.assertIn(("a", "data/extra.txt"), self.checks(rep, "WARN"))

    def test_write_assets_md(self):
        self.put("data/model/soldier0.bmp", bmp8())
        out = os.path.join(self.tmp, "ASSETS.md")
        self.assertEqual(vp.write_assets_md(self.provenance, out), 1)
        with open(out, encoding="utf-8") as f:
            text = f.read()
        self.assertIn("`data/model/soldier0.bmp`", text)
        self.assertIn("CC0-1.0", text)


class TestPresence(PackFixture):
    def test_case_insensitive_match(self):
        self.put("DATA/Model/SOLDIER0.BMP", bmp8(), provenance=False)
        self.add_provenance("data/model/soldier0.bmp",
                            hashlib.sha256(bmp8()).hexdigest())
        rep = self.run_validate()
        self.assertEqual(rep.slot_state["data/model/soldier0.bmp"], "present")
        self.assertEqual(rep.failures, [])

    def test_missing_p0_fails_missing_p1_warns(self):
        self.write_manifest([slot("data/a.bmp", prio="P0"), slot("data/b.bmp", prio="P1")])
        rep = self.run_validate()
        self.assertIn(("a", "data/a.bmp"), self.checks(rep))
        self.assertIn(("a", "data/b.bmp"), self.checks(rep, "WARN"))
        self.assertNotIn(("a", "data/b.bmp"), self.checks(rep))
        self.assertIn(("a", "data/b.bmp"), self.checks(self.run_validate(strict=True)))

    def test_empty_pack_exits_1(self):
        with redirect_stdout(io.StringIO()):
            code = vp.main([self.pack, "--manifest", self.manifest, "--reference", self.reference,
                            "--provenance", self.provenance])
        self.assertEqual(code, 1)


class TestOtherFormats(PackFixture):
    def test_wav_must_be_pcm(self):
        self.write_manifest([slot("data/sound/a.wav", kind="sound", cls="sfx", size=None,
                                  fmt="wav_pcm16_mono_22k")])
        self.put("data/sound/a.wav", wav(tag=3, bits=32))     # IEEE float
        self.assertIn(("e", "data/sound/a.wav"), self.checks(self.run_validate()))

    def test_pcm_wav_passes(self):
        self.write_manifest([slot("data/sound/a.wav", kind="sound", cls="sfx", size=None,
                                  fmt="wav_pcm16_mono_22k")])
        self.put("data/sound/a.wav", wav())
        self.assertEqual(self.run_validate().failures, [])

    def test_binary_x_rejected(self):
        self.write_manifest([slot("data/model/a.x", kind="model", cls="model", size=None, fmt="x_text")])
        self.put("data/model/a.x", b"xof 0302bin 0032")
        self.assertIn(("b", "data/model/a.x"), self.checks(self.run_validate()))

    def test_wrong_texture_header_rejected(self):
        self.put("data/model/soldier0.bmp", b"GIF89a" + b"\0" * 64)
        self.assertIn(("b", "data/model/soldier0.bmp"), self.checks(self.run_validate()))

    def test_bd1_tiles_become_slots(self):
        names = [b"wall.bmp"] + [b""] * 9
        header = b"".join(n.ljust(31, b"\0") for n in names) + struct.pack("<H", 0)
        self.write_manifest([slot("data/map1/temp.bd1", kind="map", cls="other", size=None, fmt="bd1")])
        self.put("data/map1/temp.bd1", header)
        rep = self.run_validate()
        self.assertEqual(rep.slot_state.get("data/map1/wall.bmp"), "missing")
        self.put("data/map1/WALL.bmp", bmp8(128, 128))
        rep = self.run_validate()
        self.assertEqual(rep.slot_state.get("data/map1/wall.bmp"), "present")
        self.assertEqual(rep.failures, [])


if __name__ == "__main__":
    unittest.main()

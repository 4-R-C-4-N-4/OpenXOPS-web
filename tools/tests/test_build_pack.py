"""Tests for tools/build_pack.py: staging pipeline, never writes assets/pack."""
import importlib.util
import os
import shutil
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, ".."))
ROOT = os.path.normpath(os.path.join(TOOLS_DIR, ".."))
PACK_DIR = os.path.join(ROOT, "assets", "pack")
DATA_DIR = os.path.join(PACK_DIR, "data")

_spec = importlib.util.spec_from_file_location(
    "build_pack", os.path.join(TOOLS_DIR, "build_pack.py"))
build_pack = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(build_pack)


def _snapshot(path):
    """dir -> {relpath: bytes} for everything under path."""
    out = {}
    for dirpath, _dirs, names in os.walk(path):
        for name in names:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, path)
            with open(full, "rb") as fh:
                out[rel] = fh.read()
    return out


class TestBuildPack(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="build_pack_test_")
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.staging = os.path.join(self.tmp, "staging")
        self.out = os.path.join(self.tmp, "out")

    def test_never_writes_assets_pack(self):
        before = _snapshot(PACK_DIR)
        staged_data = build_pack.stage_data(self.staging)
        build_pack.generate_placeholders(staged_data, self.staging, dry_run=False)
        build_pack.build_zip(staged_data, self.out)
        after = _snapshot(PACK_DIR)
        self.assertEqual(before, after, "assets/pack must never be modified")

    def test_deterministic_zip(self):
        staged_data = build_pack.stage_data(self.staging)
        build_pack.generate_placeholders(staged_data, self.staging, dry_run=False)
        path1, digest1 = build_pack.build_zip(staged_data, self.out)

        staging2 = os.path.join(self.tmp, "staging2")
        out2 = os.path.join(self.tmp, "out2")
        staged_data2 = build_pack.stage_data(staging2)
        build_pack.generate_placeholders(staged_data2, staging2, dry_run=False)
        path2, digest2 = build_pack.build_zip(staged_data2, out2)

        self.assertEqual(digest1, digest2)
        with open(path1, "rb") as a, open(path2, "rb") as b:
            self.assertEqual(a.read(), b.read())

    def test_real_file_wins_over_placeholder(self):
        staged_data = build_pack.stage_data(self.staging)
        manifest = build_pack.load_manifest()
        p0_file_slots = [
            s for s in manifest["slots"]
            if s.get("priority") == "P0" and not s["path"].endswith("/")
            and s["kind"] not in ("map", "points", "mission")  # never stubbed
            and (s["kind"], s.get("ref_format")) in build_pack.PLACEHOLDER_GENERATORS
        ]
        self.assertTrue(p0_file_slots)
        slot = p0_file_slots[0]
        rel = slot["path"]
        rel_under_data = rel[len("data/"):] if rel.startswith("data/") else rel
        dest = os.path.join(staged_data, rel_under_data)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        sentinel = b"REAL-PACK-FILE-SENTINEL"
        with open(dest, "wb") as fh:
            fh.write(sentinel)

        written = build_pack.generate_placeholders(staged_data, self.staging, dry_run=False)
        self.assertNotIn(rel, written)
        with open(dest, "rb") as fh:
            self.assertEqual(fh.read(), sentinel)

    def test_placeholders_fill_p0_gaps(self):
        staged_data = build_pack.stage_data(self.staging)
        written = build_pack.generate_placeholders(staged_data, self.staging, dry_run=False)
        manifest = build_pack.load_manifest()
        missing_before = [
            s for s in manifest["slots"]
            if s.get("priority") == "P0" and not s["path"].endswith("/")
            and s["kind"] not in ("map", "points", "mission")  # never stubbed
            and (s["kind"], s.get("ref_format")) in build_pack.PLACEHOLDER_GENERATORS
            and not os.path.exists(os.path.join(
                DATA_DIR, s["path"][len("data/"):] if s["path"].startswith("data/") else s["path"]))
        ]
        self.assertEqual(sorted(written), sorted(s["path"] for s in missing_before))
        self.assertFalse([p for p in written if p.endswith((".bd1", ".pd1"))],
                         "stub maps would advertise fake missions")
        for rel in written:
            rel_under_data = rel[len("data/"):] if rel.startswith("data/") else rel
            self.assertTrue(os.path.exists(os.path.join(staged_data, rel_under_data)))

    def test_valid_stub_formats(self):
        bmp = build_pack.make_bmp8(4, 4)
        self.assertEqual(bmp[0:2], b"BM")
        dds8 = build_pack.make_dds_argb8(4, 4)
        self.assertEqual(dds8[0:4], b"DDS ")
        dds4 = build_pack.make_dds_argb4(4, 4)
        self.assertEqual(dds4[0:4], b"DDS ")
        wav = build_pack.make_wav_silence(0.1)
        self.assertEqual(wav[0:4], b"RIFF")
        self.assertEqual(wav[8:12], b"WAVE")
        self.assertTrue(build_pack.CUBE_X.startswith("xof 0302txt 0032"))
        bd1 = build_pack.make_bd1_floor()
        self.assertEqual(len(bd1), 10 * 31 + 2 + 316)
        pd1 = build_pack.make_pd1_spawn()
        self.assertGreater(len(pd1), 2)

    def test_zip_structure_and_naming(self):
        staged_data = build_pack.stage_data(self.staging)
        build_pack.generate_placeholders(staged_data, self.staging, dry_run=False)
        out_path, digest = build_pack.build_zip(staged_data, self.out)
        self.assertTrue(os.path.basename(out_path).startswith("openxops-data."))
        self.assertEqual(len(digest), 16)
        with zipfile.ZipFile(out_path) as zf:
            names = zf.namelist()
            self.assertTrue(names)
            self.assertEqual(names, sorted(names))
            for n in names:
                self.assertTrue(n.startswith("data/"))

    def test_staging_rejected_inside_assets_pack(self):
        orig_argv = sys.argv
        try:
            sys.argv = ["build_pack.py", "--staging", os.path.join(PACK_DIR, "evil-staging"), "--out", self.out]
            with self.assertRaises(SystemExit):
                build_pack.main()
        finally:
            sys.argv = orig_argv


if __name__ == "__main__":
    unittest.main()

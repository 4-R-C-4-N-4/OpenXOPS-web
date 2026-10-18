"""Unit tests for tools/sync_manifest_status.py (stdlib unittest, synthetic fixtures only)."""
import hashlib
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..")))
import sync_manifest_status as sync_mod  # noqa: E402


def slot(path, kind="texture", status="missing"):
    return {"path": path, "kind": kind, "class": "other", "hard": False, "source_refs": [],
            "ref_size": None, "ref_format": None, "alpha": None, "tileable": False,
            "uv_layout": None, "summary": "test", "priority": "P2", "status": status, "note": None}


class SyncFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="sync-test-")
        self.pack = os.path.join(self.tmp, "pack")
        os.makedirs(self.pack)
        self.manifest = os.path.join(self.tmp, "manifest.json")
        self.provenance = os.path.join(self.tmp, "provenance.jsonl")
        open(self.provenance, "w").close()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def write_manifest(self, slots):
        with open(self.manifest, "w") as f:
            json.dump({"version": 1, "slots": slots}, f)

    def put(self, rel, data, provenance=True):
        full = os.path.join(self.pack, *rel.split("/"))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)
        if provenance:
            self.add_provenance(rel, hashlib.sha256(data).hexdigest())

    def add_provenance(self, rel, sha):
        rec = {"path": rel, "sha256": sha}
        with open(self.provenance, "a") as f:
            f.write(json.dumps(rec) + "\n")

    def load(self):
        with open(self.manifest, encoding="utf-8") as f:
            return json.load(f)


class TestSyncStatus(SyncFixture):
    def test_present_with_provenance_becomes_generated(self):
        self.write_manifest([slot("data/a.bmp", status="missing")])
        self.put("data/a.bmp", b"hello")
        sync_mod.sync(self.pack, self.manifest, self.provenance)
        self.assertEqual(self.load()["slots"][0]["status"], "generated")

    def test_absent_file_stays_missing(self):
        self.write_manifest([slot("data/a.bmp", status="generated")])
        sync_mod.sync(self.pack, self.manifest, self.provenance)
        self.assertEqual(self.load()["slots"][0]["status"], "missing")

    def test_present_without_provenance_is_missing(self):
        self.write_manifest([slot("data/a.bmp", status="generated")])
        self.put("data/a.bmp", b"hello", provenance=False)
        sync_mod.sync(self.pack, self.manifest, self.provenance)
        self.assertEqual(self.load()["slots"][0]["status"], "missing")

    def test_sha_mismatch_is_missing(self):
        self.write_manifest([slot("data/a.bmp", status="generated")])
        self.put("data/a.bmp", b"hello", provenance=False)
        self.add_provenance("data/a.bmp", "ab" * 32)
        sync_mod.sync(self.pack, self.manifest, self.provenance)
        self.assertEqual(self.load()["slots"][0]["status"], "missing")

    def test_dir_slot_present_is_generated_without_provenance(self):
        self.write_manifest([slot("data/briefing/", kind="dir", status="missing")])
        os.makedirs(os.path.join(self.pack, "data", "briefing"))
        sync_mod.sync(self.pack, self.manifest, self.provenance)
        self.assertEqual(self.load()["slots"][0]["status"], "generated")

    def test_dry_run_does_not_write(self):
        self.write_manifest([slot("data/a.bmp", status="missing")])
        self.put("data/a.bmp", b"hello")
        sync_mod.sync(self.pack, self.manifest, self.provenance, dry_run=True)
        self.assertEqual(self.load()["slots"][0]["status"], "missing")


class TestManifestInSync(unittest.TestCase):
    """Real-repo invariant: status 'generated' iff the file actually exists."""

    def test_real_manifest_matches_pack_state(self):
        root = os.path.normpath(os.path.join(HERE, "..", ".."))
        manifest_path = os.path.join(root, "assets", "manifest.json")
        provenance_path = os.path.join(root, "assets", "provenance.jsonl")
        pack_dir = os.path.join(root, "assets", "pack")
        if not os.path.isfile(manifest_path):
            self.skipTest("no assets/manifest.json in this checkout")

        changes = sync_mod.sync(pack_dir, manifest_path, provenance_path, dry_run=True)
        self.assertEqual(changes, [],
                          "manifest status is stale for: %s (run tools/sync_manifest_status.py)"
                          % ", ".join(p for p, _, _ in changes))


if __name__ == "__main__":
    unittest.main()

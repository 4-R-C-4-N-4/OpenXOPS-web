#!/usr/bin/env python3
"""Sync assets/manifest.json slot statuses with the actual pack contents.

For every slot:
  - status = "generated" if the file/dir exists under <pack_dir> AND (for
    files) it has a matching provenance.jsonl record (sha256 matches).
  - status = "missing" otherwise.

Dir-kind slots only need to exist (provenance applies to the files inside,
not the directory itself). Implicit map-tile slots are not part of
manifest.json and are skipped.

Usage:
    python3 tools/sync_manifest_status.py [pack_dir] [--manifest F] [--provenance F] [--dry-run]
"""
import argparse
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DEF_PACK = os.path.join(ROOT, "assets", "pack")
DEF_MANIFEST = os.path.join(ROOT, "assets", "manifest.json")
DEF_PROVENANCE = os.path.join(ROOT, "assets", "provenance.jsonl")


def load_provenance(path):
    recs = {}
    if not os.path.exists(path):
        return recs
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if isinstance(r, dict) and r.get("path"):
                recs[r["path"].replace("\\", "/").lower()] = r
    return recs


def file_exists_ci(pack_dir, rel):
    """Case-insensitive existence check, mirroring validate_pack's walk."""
    full = os.path.join(pack_dir, *rel.split("/"))
    if os.path.exists(full):
        return full
    d = os.path.dirname(full)
    base = os.path.basename(full)
    if not os.path.isdir(d):
        return None
    for name in os.listdir(d):
        if name.lower() == base.lower():
            return os.path.join(d, name)
    return None


def compute_status(pack_dir, rel, kind, prov):
    if kind == "dir":
        full = os.path.join(pack_dir, *rel.rstrip("/").split("/"))
        return "generated" if os.path.isdir(full) else "missing"
    full = file_exists_ci(pack_dir, rel)
    if full is None:
        return "missing"
    r = prov.get(rel.lower())
    if r is None:
        return "missing"
    with open(full, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    if str(r.get("sha256", "")).lower() != sha:
        return "missing"
    return "generated"


def sync(pack_dir, manifest_path, provenance_path, dry_run=False):
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)
    prov = load_provenance(provenance_path)

    changes = []
    for s in manifest["slots"]:
        new_status = compute_status(pack_dir, s["path"], s.get("kind"), prov)
        old_status = s.get("status")
        if old_status != new_status:
            changes.append((s["path"], old_status, new_status))
        s["status"] = new_status

    if not dry_run:
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)
            f.write("\n")

    return changes


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pack_dir", nargs="?", default=DEF_PACK)
    ap.add_argument("--manifest", default=DEF_MANIFEST)
    ap.add_argument("--provenance", default=DEF_PROVENANCE)
    ap.add_argument("--dry-run", action="store_true", help="report changes without writing")
    a = ap.parse_args(argv)

    changes = sync(a.pack_dir, a.manifest, a.provenance, a.dry_run)
    for path, old, new in changes:
        print("%s: %s -> %s" % (path, old, new))
    print("%d slot(s) %s" % (len(changes), "would change" if a.dry_run else "changed"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

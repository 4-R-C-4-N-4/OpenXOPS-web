"""CLI: python3 -m tools.modelgen --priority P0 -o assets/pack/data/model

Generates every model slot in tools/modelgen/specs.py matching --priority,
writes the text .x file under -o (joined with the slot's relative path),
writes a UV-layout wireframe PNG template under assets/work/uv/, and appends
one provenance.jsonl line per generated file (contract: license CC0-1.0,
generator=modelgen).
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import sys

from . import generators
from .specs import SPECS, ModelSpec
from .uv_atlas import render_uv_template
from .xwriter import Mesh, write_x

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_PROVENANCE = os.path.join(REPO_ROOT, "assets", "provenance.jsonl")
DEFAULT_UV_DIR = os.path.join(REPO_ROOT, "assets", "work", "uv")


def slot_rel_path(spec: ModelSpec) -> str:
    """Path relative to the -o root. Model slots are e.g. 'model/up0.x' or
    'model/weapon/de.x' or 'sky/sky.x'; -o is expected to be
    assets/pack/data/model, so strip the leading 'model/' when present, and
    for the sky slot caller passes assets/pack/data and we honour the full
    relative path."""
    return spec.slot


def mesh_name_for(key: str) -> str:
    return key


def _read_bytes(path: str) -> bytes:
    with open(path, "rb") as fh:
        return fh.read()


def generate_one(key: str, spec: ModelSpec):
    fn = generators.GENERATORS[spec.generator]
    mesh, atlas = fn(**spec.gen_args)
    return mesh, atlas


def write_provenance(path: str, entries) -> None:
    with open(path, "a", encoding="utf-8") as fh:
        for e in entries:
            fh.write(json.dumps(e, sort_keys=False) + "\n")


def run(priority: str, out_dir: str, provenance_path: str = DEFAULT_PROVENANCE,
        uv_dir: str = DEFAULT_UV_DIR, keys=None) -> list:
    """Generate and write every matching spec. Returns a list of
    (key, out_path, poly_count) for reporting/tests."""
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(uv_dir, exist_ok=True)
    results = []
    prov_entries = []
    date = datetime.date.today().isoformat()

    selected = keys or [k for k, s in SPECS.items() if priority in (None, "ALL") or s.priority == priority]
    for key in sorted(selected):
        spec = SPECS[key]
        mesh, atlas = generate_one(key, spec)

        # The CLI is invoked with -o assets/pack/data/model (per
        # docs/OPEN-ASSETS-SOW.md Phase B). Model slots are
        # "model/<file>.x" relative to assets/pack/data, so resolve every
        # slot against that data root (parent of -o when -o ends in
        # "model"), not against -o directly -- this lets "sky/sky.x" land
        # in .../data/sky/ instead of .../data/model/sky/.
        out_dir_norm = out_dir.rstrip(os.sep)
        if os.path.basename(out_dir_norm) == "model":
            data_root = os.path.dirname(out_dir_norm)
        else:
            data_root = out_dir_norm
        out_path = os.path.join(data_root, spec.slot)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        write_x(out_path, mesh_name_for(key), [mesh])

        uv_path = os.path.join(uv_dir, f"{key}.png")
        render_uv_template(atlas, uv_path)

        sha = hashlib.sha256(_read_bytes(out_path)).hexdigest()
        prov_entries.append({
            "path": os.path.relpath(out_path, os.path.join(REPO_ROOT, "assets", "pack")).replace(os.sep, "/"),
            "sha256": sha,
            "generator": "tools/modelgen (Python stdlib, deterministic procedural .x writer)",
            "recipe": {
                "slot": spec.slot,
                "kind": spec.kind,
                "generator_fn": spec.generator,
                "gen_args": spec.gen_args,
                "poly_count": mesh.poly_count,
                "source_refs": spec.source_refs,
            },
            "license": "CC0-1.0",
            "author": "OpenXOPS-web contributors (AI-assisted procedural generation)",
            "date": date,
            "curated_by": None,
            "prompt": f"Procedurally build a low-poly {spec.kind} model for slot {spec.slot}: {spec.summary}",
        })
        results.append((key, out_path, mesh.poly_count))

    if prov_entries:
        write_provenance(provenance_path, prov_entries)
    return results


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--priority", default="P0", help="P0, P1, or ALL")
    ap.add_argument("-o", "--out", required=True, help="output dir for .x files (e.g. assets/pack/data/model)")
    ap.add_argument("--provenance", default=DEFAULT_PROVENANCE)
    ap.add_argument("--uv-dir", default=DEFAULT_UV_DIR)
    ap.add_argument("--only", nargs="*", help="generate only these spec keys")
    a = ap.parse_args(argv)

    results = run(a.priority, a.out, a.provenance, a.uv_dir, keys=a.only)
    for key, path, polys in results:
        print(f"{key:12s} {polys:4d} polys -> {path}")
    print(f"Generated {len(results)} model(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())

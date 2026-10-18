"""CLI entry point: ``python3 -m tools.mapgen build map.json -o outdir/``
and ``python3 -m tools.mapgen dump <file.bd1|file.pd1>``.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import author, bd1, pd1


def cmd_build(args: argparse.Namespace) -> int:
    doc = json.loads(Path(args.map_json).read_text())
    result = author.build(doc)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    bd1_path = outdir / args.bd1_name
    pd1_path = outdir / args.pd1_name
    bd1_path.write_bytes(result.bd1_bytes)
    pd1_path.write_bytes(result.pd1_bytes)
    print(f"wrote {bd1_path} ({len(result.bd1_bytes)} bytes)")
    print(f"wrote {pd1_path} ({len(result.pd1_bytes)} bytes)")
    return 0


def cmd_dump(args: argparse.Namespace) -> int:
    path = Path(args.file)
    data = path.read_bytes()
    suffix = path.suffix.lower()
    if suffix == ".bd1":
        doc = author.dump_bd1(data)
    elif suffix == ".pd1":
        doc = author.dump_pd1(data)
    else:
        print(f"error: unrecognized extension {suffix!r} (expected .bd1 or .pd1)", file=sys.stderr)
        return 1
    print(json.dumps(doc, indent=2))
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="python3 -m tools.mapgen")
    sub = parser.add_subparsers(dest="command", required=True)

    p_build = sub.add_parser("build", help="compile a map.json into .bd1 + .pd1")
    p_build.add_argument("map_json")
    p_build.add_argument("-o", "--outdir", default=".")
    p_build.add_argument("--bd1-name", default="temp.bd1")
    p_build.add_argument("--pd1-name", default="mission.pd1")
    p_build.set_defaults(func=cmd_build)

    p_dump = sub.add_parser("dump", help="print an existing .bd1/.pd1 file as JSON")
    p_dump.add_argument("file")
    p_dump.set_defaults(func=cmd_dump)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())

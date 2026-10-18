#!/usr/bin/env python3
"""Build the free (CC0) data pack for OpenXOPS-web.

Two jobs:
  1. ``--placeholders``: fill every P0 manifest slot missing from
     assets/pack with a minimal, engine-valid generated file (checkerboard
     BMP/DDS, silent WAV, a unit-cube .x model, a one-block .bd1 floor, a
     one-spawn .pd1, a one-line mission .txt). Every generated file is
     recorded in assets/pack/PROVENANCE-PLACEHOLDERS.json as
     ``generator: build_pack.py``, ``license: CC0-1.0``, ``placeholder: true``.
     These are scaffolding so the engine boots with zero original data; they
     are not meant to ship as "final" art (docs/OPEN-ASSETS-SOW.md Phase C).
  2. Default (no flag): zip assets/pack/data into a deterministic archive
     ``openxops-data.<sha16>.zip`` (sorted names, fixed per-entry timestamp,
     stored/deflate only) written to --out (default build-web/).

Usage:
  python3 tools/build_pack.py --placeholders    # fill missing P0 slots
  python3 tools/build_pack.py --out build-web    # build the zip
"""
import argparse
import hashlib
import json
import os
import struct
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
PACK_DIR = os.path.join(ROOT, "assets", "pack")
DATA_DIR = os.path.join(PACK_DIR, "data")
MANIFEST_PATH = os.path.join(ROOT, "assets", "manifest.json")
PROVENANCE_PATH = os.path.join(PACK_DIR, "PROVENANCE-PLACEHOLDERS.json")

FIXED_DATETIME = (1980, 1, 1, 0, 0, 0)  # deterministic zip timestamps

# --------------------------------------------------------------------------
# Generic placeholder generators (docs/ASSET-CONTRACT.md formats)
# --------------------------------------------------------------------------

def make_bmp8(width, height):
    """Uncompressed 8-bit, 256-entry palette BMP; 2x2 grey checkerboard."""
    palette = bytearray()
    for i in range(256):
        shade = 96 if i % 2 == 0 else 160
        palette += bytes((shade, shade, shade, 0))
    row_padded = ((width + 3) // 4) * 4  # 8-bit rows padded to 4 bytes
    rows = []
    for y in range(height):
        row = bytearray(row_padded)
        for x in range(width):
            row[x] = 0 if ((x // 8) + (y // 8)) % 2 == 0 else 1
        rows.append(bytes(row))
    pixel_data = b"".join(rows)  # bottom-up order not required for a flat checker
    info_header = struct.pack(
        "<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(pixel_data), 0, 0, 256, 0
    )
    file_size = 14 + 40 + len(palette) + len(pixel_data)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 14 + 40 + len(palette))
    return header + info_header + bytes(palette) + pixel_data


def make_dds_argb8(width, height):
    """Uncompressed 32-bit A8R8G8B8 DDS; 2x2 opaque grey checkerboard."""
    dwFlags = 0x1 | 0x2 | 0x4 | 0x1000  # CAPS|HEIGHT|WIDTH|PIXELFORMAT
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, dwFlags)
    struct.pack_into("<I", header, 12, height)
    struct.pack_into("<I", header, 16, width)
    # pixel format block at offset 76 (dds header + 'DDS ' magic is separate 4 bytes
    # before the 124-byte header per docs; engine reads absolute file offsets)
    struct.pack_into("<I", header, 0x58, 32)  # dwRGBBitCount
    struct.pack_into("<I", header, 0x6C, 0x1000)  # dwCaps: TEXTURE
    pixels = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            o = (y * width + x) * 4
            shade = 160 if ((x // 8) + (y // 8)) % 2 == 0 else 96
            pixels[o:o + 4] = bytes((shade, shade, shade, 255))  # B,G,R,A
    return bytes(header) + bytes(pixels)


def make_dds_argb4(width, height):
    """Uncompressed 16-bit A4R4G4B4 DDS, used for char.dds (alpha-capable)."""
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, 0x1 | 0x2 | 0x4 | 0x1000)
    struct.pack_into("<I", header, 12, height)
    struct.pack_into("<I", header, 16, width)
    struct.pack_into("<I", header, 0x58, 16)
    struct.pack_into("<I", header, 0x6C, 0x1000)
    pixels = bytearray(width * height * 2)
    for y in range(height):
        for x in range(width):
            o = (y * width + x) * 2
            # space (0x20) cell must be fully transparent per ASSET-CONTRACT 2.1;
            # a uniform fully-opaque white glyph atlas keeps every cell usable
            # without extra logic for a placeholder.
            pixels[o:o + 2] = struct.pack("<H", 0xFFFF)
    return bytes(header) + bytes(pixels)


def make_wav_silence(duration_s=0.3, rate=22050):
    n = int(duration_s * rate)
    data = b"\x00\x00" * n
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks


CUBE_X = """xof 0302txt 0032

Mesh Cube {
24;
-1.000000;2.000000;-1.000000;,
1.000000;2.000000;-1.000000;,
1.000000;0.000000;-1.000000;,
-1.000000;0.000000;-1.000000;,
1.000000;2.000000;1.000000;,
-1.000000;2.000000;1.000000;,
-1.000000;0.000000;1.000000;,
1.000000;0.000000;1.000000;,
-1.000000;2.000000;1.000000;,
-1.000000;2.000000;-1.000000;,
-1.000000;0.000000;-1.000000;,
-1.000000;0.000000;1.000000;,
1.000000;2.000000;-1.000000;,
1.000000;2.000000;1.000000;,
1.000000;0.000000;1.000000;,
1.000000;0.000000;-1.000000;,
-1.000000;2.000000;1.000000;,
1.000000;2.000000;1.000000;,
1.000000;2.000000;-1.000000;,
-1.000000;2.000000;-1.000000;,
-1.000000;0.000000;-1.000000;,
1.000000;0.000000;-1.000000;,
1.000000;0.000000;1.000000;,
-1.000000;0.000000;1.000000;;

6;
4;0,1,2,3;,
4;4,5,6,7;,
4;8,9,10,11;,
4;12,13,14,15;,
4;16,17,18,19;,
4;20,21,22,23;;

 MeshTextureCoords {
24;
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;;
 }

 MeshMaterialList {
1;
6;
0,
0,
0,
0,
0,
0;;
 Material {
1.000000;1.000000;1.000000;1.000000;;
0.000000;
0.000000;0.000000;0.000000;;
0.000000;0.000000;0.000000;;
 }
 }
}
"""


def make_bd1_floor(texture_name=b""):
    """One flat 100x100 floor block, per docs/ASSET-CONTRACT.md 9.1."""
    textures = bytearray(10 * 31)
    if texture_name:
        name = texture_name[:30]
        textures[0:len(name)] = name
    half = 50.0
    # 8 vertices of a thin box, y in [-1, 0], matching .bd1 vertex layout
    xs = [-half, half, half, -half, -half, half, half, -half]
    ys = [0, 0, 0, 0, -1, -1, -1, -1]
    zs = [-half, -half, half, half, -half, -half, half, half]
    uv = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0] * 6  # u[6][4]+v[6][4] reused flat
    geom = struct.pack("<8f", *xs) + struct.pack("<8f", *ys) + struct.pack("<8f", *zs)
    geom += struct.pack("<48f", *uv)
    assert len(geom) == 288
    face_ids = bytearray(28)
    for j in range(6):
        face_ids[4 * j] = 0  # textureID 0 (signed byte, top slot used for all faces)
    block = geom + bytes(face_ids)
    assert len(block) == 316
    out = bytes(textures) + struct.pack("<H", 1) + block
    return out


def make_pd1_spawn():
    """One HUMAN spawn point at the origin (docs/ASSET-CONTRACT.md 9.2)."""
    record = struct.pack("<4f4b", 0.0, 0.0, 0.0, 0.0, 1, 0, 0, 0)  # p1=1 HUMAN
    return struct.pack("<H", 1) + record


PLACEHOLDER_GENERATORS = {
    ("texture", "dds_argb8"): lambda size: make_dds_argb8(*size),
    ("texture", "dds_argb4"): lambda size: make_dds_argb4(*size),
    ("texture", "bmp8"): lambda size: make_bmp8(*size),
    ("model", "x_text"): lambda size: CUBE_X.encode("ascii"),
    ("map", "bd1"): lambda size: make_bd1_floor(),
    ("points", "pd1"): lambda size: make_pd1_spawn(),
    ("mission", "text_cp932"): lambda size: (
        b"!\n!\n1\n" + b"Stub placeholder mission (CC0, generated).\n"
    ),
    ("sound", "wav_pcm16_mono_22k"): lambda size: make_wav_silence(),
}

DEFAULT_SIZE = {
    "dds_argb8": (128, 128),
    "dds_argb4": (512, 512),
    "bmp8": (128, 128),
}


def load_manifest():
    with open(MANIFEST_PATH, "r", encoding="utf-8") as fh:
        return json.load(fh)


def generate_placeholders(dry_run=False):
    manifest = load_manifest()
    slots = manifest["slots"]
    provenance = []
    if os.path.exists(PROVENANCE_PATH):
        with open(PROVENANCE_PATH, "r", encoding="utf-8") as fh:
            provenance = json.load(fh)
    existing_paths = {p["path"] for p in provenance}
    written = []
    for slot in sorted(slots, key=lambda s: s["path"]):
        if slot.get("priority") != "P0":
            continue
        rel = slot["path"]
        # Manifest paths are rooted at the game's "data/" dir; DATA_DIR already
        # points at assets/pack/data, so strip that one shared prefix.
        rel_under_data = rel[len("data/"):] if rel.startswith("data/") else rel
        if rel.endswith("/"):
            dest = os.path.join(DATA_DIR, rel_under_data)
            if not dry_run:
                os.makedirs(dest, exist_ok=True)
            continue
        dest = os.path.join(DATA_DIR, rel_under_data)
        if os.path.exists(dest):
            continue  # already authored/generated; never overwrite
        key = (slot["kind"], slot.get("ref_format"))
        gen = PLACEHOLDER_GENERATORS.get(key)
        if gen is None:
            continue
        size = tuple(slot["ref_size"]) if slot.get("ref_size") else DEFAULT_SIZE.get(slot.get("ref_format"))
        content = gen(size)
        if not dry_run:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as fh:
                fh.write(content)
        written.append(rel)
        if rel not in existing_paths:
            provenance.append({
                "path": rel,
                "generator": "tools/build_pack.py --placeholders",
                "license": "CC0-1.0",
                "placeholder": True,
                "note": "Minimal valid stub so the engine boots/plays with no original data. Not final art (OPEN-ASSETS-SOW.md Phase C).",
            })
    if not dry_run and written:
        provenance.sort(key=lambda p: p["path"])
        with open(PROVENANCE_PATH, "w", encoding="utf-8") as fh:
            json.dump(provenance, fh, indent=1, sort_keys=True)
            fh.write("\n")
    return written


# --------------------------------------------------------------------------
# Deterministic zip build
# --------------------------------------------------------------------------

def collect_files(data_dir):
    files = []
    for dirpath, _dirnames, filenames in os.walk(data_dir):
        for name in filenames:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, data_dir).replace(os.sep, "/")
            files.append(rel)
    return sorted(files)


def build_zip(data_dir, out_dir):
    files = collect_files(data_dir)
    if not files:
        raise SystemExit("assets/pack/data is empty; nothing to pack")
    # First pass: content hash over sorted (name, bytes) for a stable fingerprint.
    hasher = hashlib.sha256()
    blobs = []
    for rel in files:
        with open(os.path.join(data_dir, rel), "rb") as fh:
            data = fh.read()
        blobs.append((rel, data))
        hasher.update(rel.encode("utf-8"))
        hasher.update(b"\0")
        hasher.update(data)
    digest = hasher.hexdigest()[:16]
    out_name = f"openxops-data.{digest}.zip"
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, out_name)
    tmp_path = out_path + ".tmp"
    with zipfile.ZipFile(tmp_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for rel, data in blobs:
            info = zipfile.ZipInfo("data/" + rel, date_time=FIXED_DATETIME)
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            zf.writestr(info, data)
    os.replace(tmp_path, out_path)
    return out_path, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--placeholders", action="store_true", help="fill missing P0 slots with generated stubs")
    parser.add_argument("--check", action="store_true", help="with --placeholders: report what would be written, do not write")
    parser.add_argument("--out", default=os.path.join(ROOT, "build-web"), help="output directory for the pack zip")
    args = parser.parse_args()

    if args.placeholders:
        written = generate_placeholders(dry_run=args.check)
        verb = "Would generate" if args.check else "Generated"
        print(f"{verb} {len(written)} placeholder file(s):")
        for rel in written:
            print(" ", rel)
        return

    out_path, digest = build_zip(DATA_DIR, args.out)
    print(f"Built {out_path} (sha16 {digest})")


if __name__ == "__main__":
    main()

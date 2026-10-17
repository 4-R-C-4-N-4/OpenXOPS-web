#!/usr/bin/env python3
"""Validate a data pack directory against assets/manifest.json.

Usage:
    python3 tools/validate_pack.py <pack_dir> [--strict] [--write-assets-md] [-v]
           [--manifest F] [--reference F] [--provenance F] [--assets-md F]

<pack_dir> is the directory that contains data/ (e.g. assets/pack).

Checks (FAIL makes the exit code 1; WARN only reports, unless --strict):
  a  every manifest slot is present (case-insensitive path match).
     Missing P0 slot = FAIL; missing P1/P2 = WARN.
  b  file format by header bytes: BM / DDS / FFD8 / 89PNG for textures,
     "xof 0302txt" for models, RIFF-WAVE for sounds, record sizes for .bd1/.pd1.
  c  texture dimensions vs ref_size (WARN).
  d  BMP must be uncompressed (biCompression == 0) with 4/8/24/32 bpp, bottom-up. FAIL.
     DDS must be uncompressed 16/32 bpp (no FourCC), as the GL loader reads raw pixels.
  e  WAV must be PCM (wFormatTag 1). FAIL. Other channel/rate/bit depth = WARN.
  f  clean-room guard: any file whose MD5 matches an original XOPS 0.96 file. FAIL.
  g  every file has a record in provenance.jsonl with a matching sha256. FAIL.

--write-assets-md regenerates assets/ASSETS.md (credits table) from provenance.jsonl.
Stdlib only.
"""
import argparse
import hashlib
import json
import os
import struct
import sys
from collections import Counter, OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DEF_MANIFEST = os.path.join(ROOT, "assets", "manifest.json")
DEF_REFERENCE = os.path.join(HERE, "manifest", "xops096_reference.json")
DEF_PROVENANCE = os.path.join(ROOT, "assets", "provenance.jsonl")
DEF_ASSETS_MD = os.path.join(ROOT, "assets", "ASSETS.md")

IGNORE_NAMES = {".gitkeep"}
PROV_REQUIRED = ("path", "sha256", "generator", "license", "author", "date", "curated_by")
BD1_HEADER = 10 * 31 + 2      # texture names + block count
BD1_BLOCK = 4 * (24 + 48) + 28
PD1_HEADER = 2
PD1_POINT = 4 * 4 + 4
TEXTURE_FAMILIES = ("bmp", "dds", "jpeg", "png")


# --------------------------------------------------------------------------
# Header sniffing
# --------------------------------------------------------------------------

def sniff(head):
    if head[:2] == b"BM":
        return "bmp"
    if head[:4] == b"DDS ":
        return "dds"
    if head[:2] == b"\xff\xd8":
        return "jpeg"
    if head[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if head[:4] == b"xof ":
        return "x"
    if head[:4] == b"RIFF" and head[8:12] == b"WAVE":
        return "wav"
    return "unknown"


def bmp_info(data):
    if len(data) < 54:
        return None
    w, h = struct.unpack_from("<ii", data, 0x12)
    bpp, comp = struct.unpack_from("<HI", data, 0x1C)
    return {"width": w, "height": h, "bpp": bpp, "compression": comp}


def dds_info(data):
    if len(data) < 128:
        return None
    h, w = struct.unpack_from("<II", data, 0x0C)
    pf_flags, fourcc, bits = struct.unpack_from("<I4sI", data, 0x50)
    amask = struct.unpack_from("<I", data, 0x68)[0]
    return {"width": w, "height": h, "pf_flags": pf_flags, "fourcc": fourcc, "bpp": bits, "amask": amask}


def png_info(data):
    if len(data) < 24 or data[12:16] != b"IHDR":
        return None
    w, h = struct.unpack_from(">II", data, 16)
    return {"width": w, "height": h}


def jpeg_info(data):
    i = 2
    while i + 9 < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        marker = data[i + 1]
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            i += 2
            continue
        seglen = struct.unpack_from(">H", data, i + 2)[0]
        if marker in (0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF):
            h, w = struct.unpack_from(">HH", data, i + 5)
            return {"width": w, "height": h}
        i += 2 + seglen
    return None


def wav_info(data):
    i = 12
    while i + 8 <= len(data):
        cid = data[i:i + 4]
        size = struct.unpack_from("<I", data, i + 4)[0]
        if cid == b"fmt " and i + 24 <= len(data):
            tag, ch, rate = struct.unpack_from("<HHI", data, i + 8)
            bits = struct.unpack_from("<H", data, i + 22)[0]
            if tag == 0xFFFE and size >= 40 and i + 8 + 26 <= len(data):
                tag = struct.unpack_from("<H", data, i + 8 + 24)[0]   # WAVE_FORMAT_EXTENSIBLE subformat
                return {"tag": tag, "channels": ch, "rate": rate, "bits": bits, "extensible": True}
            return {"tag": tag, "channels": ch, "rate": rate, "bits": bits, "extensible": False}
        i += 8 + size + (size & 1)
    return None


def bd1_textures(data):
    names = []
    for i in range(10):
        raw = data[i * 31:(i + 1) * 31].split(b"\0", 1)[0]
        names.append(raw.decode("cp932", "replace").strip())
    return names


# --------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------

class Report:
    def __init__(self, strict=False):
        self.strict = strict
        self.issues = []          # (level, check, path, message)
        self.slot_state = {}      # lower path -> "present" | "missing"
        self.slot_failed = set()

    def fail(self, check, path, msg):
        self.issues.append(("FAIL", check, path, msg))
        if path:
            self.slot_failed.add(path.lower())

    def warn(self, check, path, msg):
        if self.strict:
            self.fail(check, path, msg + " [strict]")
        else:
            self.issues.append(("WARN", check, path, msg))

    @property
    def failures(self):
        return [i for i in self.issues if i[0] == "FAIL"]

    @property
    def warnings(self):
        return [i for i in self.issues if i[0] == "WARN"]


# --------------------------------------------------------------------------
# Validation
# --------------------------------------------------------------------------

def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def load_provenance(path, report):
    recs = {}
    if not os.path.exists(path):
        return recs
    with open(path, encoding="utf-8") as f:
        for no, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError as e:
                report.fail("g", None, "provenance.jsonl:%d is not valid JSON (%s)" % (no, e))
                continue
            if not isinstance(r, dict) or not r.get("path"):
                report.fail("g", None, "provenance.jsonl:%d has no path" % no)
                continue
            r["_line"] = no
            recs[r["path"].replace("\\", "/").lower()] = r      # last record for a path wins
    return recs


def walk_pack(pack_dir):
    files = []
    for dp, dns, fns in os.walk(pack_dir):
        dns.sort()
        for fn in sorted(fns):
            if fn in IGNORE_NAMES:
                continue
            full = os.path.join(dp, fn)
            files.append(os.path.relpath(full, pack_dir).replace(os.sep, "/"))
    return files


def check_texture(rep, rel, data, slot):
    fam = sniff(data[:16])
    if fam not in TEXTURE_FAMILIES:
        rep.fail("b", rel, "texture header is %s; engine accepts BM/DDS/JPEG/PNG" % fam)
        return
    ref_fmt = (slot or {}).get("ref_format") or ""
    info = None
    if fam == "bmp":
        info = bmp_info(data)
        if info is None:
            rep.fail("b", rel, "truncated BMP header")
            return
        if info["compression"] != 0:
            rep.fail("d", rel, "BMP biCompression=%d (RLE/bitfields); must be 0 (use -compress None)"
                     % info["compression"])
        if info["bpp"] not in (4, 8, 24, 32):
            rep.fail("d", rel, "BMP %d bpp; engine loader handles 4/8/24/32" % info["bpp"])
        if info["height"] <= 0 or info["width"] <= 0:
            rep.fail("d", rel, "BMP must be bottom-up with positive size (got %dx%d)"
                     % (info["width"], info["height"]))
        got_fmt = "bmp%d" % info["bpp"]
    elif fam == "dds":
        info = dds_info(data)
        if info is None:
            rep.fail("b", rel, "truncated DDS header")
            return
        if info["pf_flags"] & 0x4:
            rep.fail("d", rel, "DDS is FourCC-compressed (%r); loader reads raw 16/32 bpp only"
                     % info["fourcc"])
        if info["bpp"] not in (16, 32):
            rep.fail("d", rel, "DDS %d bpp; loader reads 16 or 32 bpp" % info["bpp"])
        got_fmt = {32: "dds_argb8", 16: "dds_argb4"}.get(info["bpp"], "dds")
    elif fam == "png":
        info = png_info(data)
        got_fmt = "png"
    else:
        info = jpeg_info(data)
        got_fmt = "jpeg"
    if ref_fmt and got_fmt != ref_fmt:
        rep.warn("b", rel, "format %s differs from reference %s" % (got_fmt, ref_fmt))
    ref_size = (slot or {}).get("ref_size")
    if info and ref_size and [abs(info["width"]), abs(info["height"])] != list(ref_size):
        rep.warn("c", rel, "size %dx%d, reference %dx%d" % (abs(info["width"]), abs(info["height"]),
                                                             ref_size[0], ref_size[1]))


def check_file(rep, rel, data, slot, kind):
    if kind == "texture":
        check_texture(rep, rel, data, slot)
    elif kind == "model":
        if not data.startswith(b"xof 0302txt"):
            rep.fail("b", rel, "model must start with 'xof 0302txt' (text DirectX .x)")
    elif kind == "sound":
        if sniff(data[:16]) != "wav":
            rep.fail("b", rel, "sound is not RIFF/WAVE")
            return
        w = wav_info(data)
        if w is None:
            rep.fail("e", rel, "WAV has no fmt chunk")
        elif w["tag"] != 1:
            rep.fail("e", rel, "WAV format tag %d; must be PCM (1)" % w["tag"])
        elif (w["channels"], w["rate"], w["bits"]) != (1, 22050, 16):
            rep.warn("e", rel, "WAV %dch %dHz %dbit; reference is mono 22050Hz 16bit"
                     % (w["channels"], w["rate"], w["bits"]))
    elif kind == "map":
        if len(data) < BD1_HEADER:
            rep.fail("b", rel, ".bd1 shorter than its %d-byte header" % BD1_HEADER)
            return
        n = struct.unpack_from("<H", data, 310)[0]
        if n > 160:
            rep.fail("b", rel, ".bd1 has %d blocks; engine maximum is 160 (MAX_BLOCKS)" % n)
        if len(data) < BD1_HEADER + n * BD1_BLOCK:
            rep.fail("b", rel, ".bd1 declares %d blocks but is %d bytes" % (n, len(data)))
    elif kind == "points":
        if len(data) < PD1_HEADER:
            rep.fail("b", rel, ".pd1 shorter than its header")
            return
        n = struct.unpack_from("<H", data, 0)[0]
        if n > 256:
            rep.fail("b", rel, ".pd1 has %d points; engine maximum is 256 (MAX_POINTS)" % n)
        if len(data) < PD1_HEADER + n * PD1_POINT:
            rep.fail("b", rel, ".pd1 declares %d points but is %d bytes" % (n, len(data)))
    elif kind == "mission":
        try:
            data.decode("cp932")
        except UnicodeDecodeError:
            rep.warn("b", rel, "text is not valid CP932")


def validate(pack_dir, manifest_path=DEF_MANIFEST, reference_path=DEF_REFERENCE,
             provenance_path=DEF_PROVENANCE, strict=False):
    rep = Report(strict)
    manifest = load_json(manifest_path)
    slots = manifest["slots"]
    ref = load_json(reference_path)
    ref_md5 = {e["md5"].lower(): e["path"] for e in ref.get("files", []) if e.get("md5")}
    prov = load_provenance(provenance_path, rep)

    files = walk_pack(pack_dir) if os.path.isdir(pack_dir) else []
    index = {}
    for rel in files:
        index.setdefault(rel.lower(), []).append(rel)
    for low, rels in sorted(index.items()):
        if len(rels) > 1:
            rep.fail("a", rels[0], "case-insensitive collision: %s" % ", ".join(rels))

    slot_by_key = OrderedDict((s["path"].lower(), s) for s in slots)
    covered = set()

    # Map tiles named by the pack's own .bd1 files become implicit slots.
    for low, rels in sorted(index.items()):
        if low.endswith(".bd1"):
            with open(os.path.join(pack_dir, rels[0]), "rb") as f:
                data = f.read(BD1_HEADER)
            d = os.path.dirname(low)
            for nm in bd1_textures(data):
                if not nm:
                    continue
                tkey = (d + "/" + nm.replace("\\", "/")).lower()
                if tkey not in slot_by_key:
                    slot_by_key[tkey] = {"path": d + "/" + nm, "kind": "texture", "class": "map_tile",
                                         "ref_size": [128, 128], "ref_format": "bmp8",
                                         "priority": slot_by_key.get(low, {}).get("priority", "P2"),
                                         "_implicit": rels[0]}

    # (a) presence + (b..e) per-slot format checks
    for key, s in slot_by_key.items():
        if s["kind"] == "dir":
            present = any(k.startswith(key) for k in index) or os.path.isdir(os.path.join(pack_dir, key))
            if present and s.get("class") == "briefing":
                # Pictures named by mission scripts: any direct child is a texture slot.
                for k in sorted(index):
                    if k.startswith(key) and k.count("/") == key.count("/"):
                        covered.add(k)
                        with open(os.path.join(pack_dir, index[k][0]), "rb") as f:
                            check_texture(rep, index[k][0], f.read(),
                                          {"ref_size": [128, 128], "ref_format": "bmp8"})
        else:
            present = key in index
        rep.slot_state[key] = "present" if present else "missing"
        if not present:
            msg = "missing %s slot" % s.get("priority", "P?")
            if s.get("_implicit"):
                rep.warn("a", s["path"], "map tile named by %s is missing" % s["_implicit"])
            elif s.get("priority") == "P0":
                rep.fail("a", s["path"], msg)
            else:
                rep.warn("a", s["path"], msg)
            continue
        if s["kind"] == "dir":
            continue
        covered.add(key)
        rel = index[key][0]
        with open(os.path.join(pack_dir, rel), "rb") as f:
            data = f.read()
        check_file(rep, rel, data, s, s["kind"])

    # Every file: unlisted warning, (f) clean-room MD5, (g) provenance.
    for rel in files:
        low = rel.lower()
        if low not in covered:
            rep.warn("a", rel, "file is not a manifest slot (unlisted)")
        with open(os.path.join(pack_dir, rel), "rb") as f:
            data = f.read()
        md5 = hashlib.md5(data).hexdigest()
        if md5 in ref_md5:
            rep.fail("f", rel, "CLEAN-ROOM: md5 %s equals original XOPS file %s" % (md5, ref_md5[md5]))
        sha = hashlib.sha256(data).hexdigest()
        r = prov.get(low)
        if r is None:
            rep.fail("g", rel, "no provenance record in provenance.jsonl")
            continue
        miss = [k for k in PROV_REQUIRED if k not in r]
        if not (r.get("prompt") or r.get("source_url")):
            miss.append("prompt|source_url")
        if miss:
            rep.fail("g", rel, "provenance line %d lacks: %s" % (r["_line"], ", ".join(miss)))
        if str(r.get("sha256", "")).lower() != sha:
            rep.fail("g", rel, "provenance sha256 mismatch (record %s, file %s)" % (r.get("sha256"), sha))
    pack_keys = set(index)
    for low, r in sorted(prov.items()):
        if low not in pack_keys:
            rep.warn("g", r["path"], "provenance record for a file not in the pack")

    return rep, slot_by_key


# --------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------

def summary_table(rep, slot_by_key):
    lines = []

    def table(title, keyfn):
        rows = OrderedDict()
        for key, s in slot_by_key.items():
            k = keyfn(s)
            r = rows.setdefault(k, Counter())
            r["total"] += 1
            r[rep.slot_state.get(key, "missing")] += 1
            if key in rep.slot_failed:
                r["failed"] += 1
        lines.append("")
        lines.append("%-14s %6s %8s %8s %7s" % (title, "total", "present", "missing", "failed"))
        for k in sorted(rows):
            r = rows[k]
            lines.append("%-14s %6d %8d %8d %7d" % (k, r["total"], r["present"], r["missing"], r["failed"]))

    table("priority", lambda s: s.get("priority", "?"))
    table("class", lambda s: s.get("class", "?"))
    table("kind", lambda s: s.get("kind", "?"))
    st = Counter(s.get("status", "missing") for s in slot_by_key.values() if not s.get("_implicit"))
    lines.append("")
    lines.append("manifest status: " + ", ".join("%s=%d" % kv for kv in sorted(st.items())))
    return "\n".join(lines)


def write_assets_md(provenance_path, out_path):
    rep = Report()
    recs = load_provenance(provenance_path, rep)
    lines = [
        "# Free Data Pack credits",
        "",
        "Generated by `python3 tools/validate_pack.py assets/pack --write-assets-md` from",
        "`assets/provenance.jsonl`. Do not edit by hand.",
        "",
        "| Path | License | Author | Generator | Prompt / source | Date | Curated by |",
        "|---|---|---|---|---|---|---|",
    ]

    def cell(v, limit=None):
        v = "" if v is None else str(v)
        v = " ".join(v.split()).replace("|", "\\|")
        if limit and len(v) > limit:
            v = v[:limit - 1] + "…"
        return v

    for low in sorted(recs):
        r = recs[low]
        src = r.get("source_url") or r.get("prompt")
        lines.append("| `%s` | %s | %s | %s | %s | %s | %s |" % (
            cell(r.get("path")), cell(r.get("license")), cell(r.get("author")), cell(r.get("generator")),
            cell(src, 120), cell(r.get("date")), cell(r.get("curated_by"))))
    if not recs:
        lines.append("| _(no assets yet)_ | | | | | | |")
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return len(recs)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Validate a Free Data Pack directory against the manifest.")
    ap.add_argument("pack_dir")
    ap.add_argument("--strict", action="store_true", help="treat warnings (size, missing P1/P2, ...) as failures")
    ap.add_argument("--write-assets-md", action="store_true", help="regenerate assets/ASSETS.md")
    ap.add_argument("-v", "--verbose", action="store_true", help="list every missing slot and warning")
    ap.add_argument("--manifest", default=DEF_MANIFEST)
    ap.add_argument("--reference", default=DEF_REFERENCE)
    ap.add_argument("--provenance", default=DEF_PROVENANCE)
    ap.add_argument("--assets-md", default=DEF_ASSETS_MD)
    a = ap.parse_args(argv)

    if not os.path.isdir(a.pack_dir):
        print("pack dir not found: %s" % a.pack_dir)
        return 1
    rep, slot_by_key = validate(a.pack_dir, a.manifest, a.reference, a.provenance, a.strict)

    print("pack: %s   slots: %d   files checked: %d" % (
        a.pack_dir, len(slot_by_key), len(walk_pack(a.pack_dir))))
    print(summary_table(rep, slot_by_key))
    print("")
    shown_missing_warn = 0
    for level, check, path, msg in rep.issues:
        quiet = level == "WARN" and check == "a" and msg.startswith("missing")
        if quiet and not a.verbose:
            shown_missing_warn += 1
            continue
        print("%s [%s] %s: %s" % (level, check, path or "-", msg))
    if shown_missing_warn:
        print("(%d missing P1/P2 slots not listed; use -v)" % shown_missing_warn)
    nf, nw = len(rep.failures), len(rep.warnings)
    print("")
    print("RESULT: %s  (%d failures, %d warnings)" % ("FAIL" if nf else "OK", nf, nw))

    if a.write_assets_md:
        n = write_assets_md(a.provenance, a.assets_md)
        print("wrote %s (%d records)" % (os.path.relpath(a.assets_md), n))
    return 1 if nf else 0


if __name__ == "__main__":
    sys.exit(main())

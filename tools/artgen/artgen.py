#!/usr/bin/env python3
"""artgen.py - Codex image-gen art pipeline for the OpenXOPS-web Free Data Pack.

Spec: docs/FREE-DATA-PACK.md section 5. Python 3 stdlib + Pillow only.

  artgen.py gen <batch.json|slot> [--n N] [--dry-run] [--force-post]
  artgen.py pick <slot> <i>
  artgen.py sheet <slot>
  artgen.py status [batch.json]
  artgen.py preview <batch.json> <out.png> [--thumb PX]
  artgen.py verify <file.bmp>...

<slot> is a pack path (data/tiles_w1/grass.bmp) or a unique bare name (grass).
"""
import argparse
import datetime as dt
import glob
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

from PIL import Image, ImageChops, ImageDraw, ImageStat
from dds import write_dds, verify_dds, engine_load


def extract_black_alpha(im):
    """Assume max(R,G,B) is coverage; unmatte black to straight RGBA.

    This is ambiguous for dark subjects: black detail cannot be recovered.
    Use authored RGBA for dark/black art (especially the scope surround).
    """
    out = Image.new("RGBA", im.size)
    pixels = []
    for r, g, b in im.convert("RGB").get_flattened_data():
        a = max(r, g, b)
        pixels.append(tuple((v * 255 + a // 2) // a for v in (r, g, b)) + (a,)
                      if a else (0, 0, 0, 0))
    out.putdata(pixels)
    return out


def verify_texture(path, slot):
    fmt = slot.get("ref_format", "bmp8")
    return (verify_dds(path, slot.get("ref_size"), fmt) if fmt.startswith("dds_")
            else verify_bmp8(path, slot.get("ref_size")))


def postprocess_dds(slot, i, raw, meta):
    wd = work_dir(slot)
    with Image.open(raw) as src:
        meta["raw_size"] = list(src.size)
        # Preserve real alpha; RGB effects explicitly opt into black extraction.
        if slot.get("alpha_source", "channel") == "black":
            im = extract_black_alpha(src)
        elif "A" in src.getbands() or "transparency" in src.info:
            im = src.convert("RGBA")
        else:
            raise ValueError("DDS needs RGBA or alpha_source=black")
        im = im.resize(tuple(slot["ref_size"]), Image.Resampling.LANCZOS)
    # Canonical transparent texels carry no hidden RGB.
    im.putdata([(r,g,b,a) if a else (0,0,0,0) for r,g,b,a in im.get_flattened_data()])
    path = os.path.join(wd, "%d.dds" % i)
    write_dds(im, path, slot["ref_format"])
    probs = verify_texture(path, slot)
    if probs:
        raise ValueError("DDS verification failed: %s" % probs)
    w, h, rgba = engine_load(path)
    final = Image.frombytes("RGBA", (w,h), rgba)
    final.save(os.path.join(wd, "%d_256.png" % i))
    tile2x2(final).save(os.path.join(wd, "%d_tiled.png" % i))
    meta.update(dds=rel(path), dds_sha256=sha256(path), dds_bytes=os.path.getsize(path))
    _write_meta(slot, i, meta)
    return meta

# --- configuration (FREE-DATA-PACK.md section 7) ---------------------------
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(REPO, "assets")
PACK_ROOT = os.environ.get("PACK_ROOT", os.path.join(ASSETS, "pack"))
WORK_ROOT = os.environ.get("WORK_ROOT", os.path.join(ASSETS, "work"))
BATCH_DIR = os.path.join(ASSETS, "batches")
MANIFEST = os.path.join(ASSETS, "manifest.json")
PROVENANCE = os.environ.get("PROVENANCE", os.path.join(ASSETS, "provenance.jsonl"))
LOG = os.path.join(WORK_ROOT, "artgen.log")
ARTGEN_CANDIDATES = int(os.environ.get("ARTGEN_CANDIDATES", "2"))
# Seam excess = (mean abs RGB diff across the wrap edge) - (mean abs RGB diff
# between adjacent interior pixels), 0-255 scale, worst of the two axes.
# Tuned on Wave 1a samples, see tools/artgen/README.md.
SEAM_MAX = float(os.environ.get("SEAM_MAX", "12.0"))
QUANT_COLORS = int(os.environ.get("QUANT_COLORS", "256"))
CODEX_TIMEOUT_S = int(os.environ.get("CODEX_TIMEOUT_S", "300"))
GENERATOR = "codex-cli 0.163 image_generation"
LICENSE = "CC0-1.0"
AUTHOR = "OpenXOPS-web contributors (AI-generated, human-curated)"
CODEX_IMG_DIR = os.path.expanduser("~/.codex/generated_images")

SLOT_FIELDS = ("path", "kind", "class", "ref_size", "ref_format", "alpha",
               "tileable", "summary")


# --- slot loading ------------------------------------------------------------
def load_batch(path):
    with open(path) as f:
        b = json.load(f)
    d = b.get("defaults", {})
    slots = []
    for s in b["slots"]:
        slot = dict(d)
        slot.update(s)
        slot["_batch"] = os.path.relpath(path, REPO)
        slots.append(slot)
    return slots


def all_known_slots():
    slots = {}
    if os.path.exists(MANIFEST):
        with open(MANIFEST) as f:
            for s in json.load(f).get("slots", []):
                if s.get("kind") == "texture":
                    slots[s["path"]] = s
    for p in sorted(glob.glob(os.path.join(BATCH_DIR, "*.json"))):
        for s in load_batch(p):
            slots[s["path"]] = s  # batch specs override manifest summaries
    return slots


def resolve_slot(name):
    slots = all_known_slots()
    if name in slots:
        return slots[name]
    hits = [s for p, s in slots.items()
            if os.path.splitext(os.path.basename(p))[0] == name]
    if len(hits) == 1:
        return hits[0]
    sys.exit("artgen: unknown or ambiguous slot %r (%d matches)" % (name, len(hits)))


def select(target):
    if target.endswith(".json") and os.path.exists(target):
        return load_batch(target)
    return [resolve_slot(target)]


def work_dir(slot):
    return os.path.join(WORK_ROOT, os.path.splitext(slot["path"])[0])


def rel(p):
    return os.path.relpath(p, REPO)


# --- prompt -----------------------------------------------------------------
def _between(path, tag):
    with open(path) as f:
        txt = f.read()
    m = re.search(r"<!-- %s:begin -->(.*?)<!-- %s:end -->" % (tag, tag), txt, re.S)
    if not m:
        sys.exit("artgen: %s lacks <!-- %s:begin/end --> markers" % (path, tag))
    return " ".join(m.group(1).split())


def build_prompt(slot):
    pre = _between(os.path.join(ASSETS, "STYLE.md"), "preamble")
    tpl_path = os.path.join(ASSETS, "prompts", slot["class"] + ".md")
    tpl = _between(tpl_path, "template")
    return pre + " " + tpl.replace("{summary}", slot["summary"].rstrip("."))


# --- logging / hashing ------------------------------------------------------
def log(msg):
    os.makedirs(WORK_ROOT, exist_ok=True)
    line = "%s %s" % (dt.datetime.now().isoformat(timespec="seconds"), msg)
    print(line)
    with open(LOG, "a") as f:
        f.write(line + "\n")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def known_raw_hashes(exclude=None):
    out = set()
    for mp in glob.glob(os.path.join(WORK_ROOT, "**", "*.json"), recursive=True):
        if mp == exclude:
            continue
        try:
            with open(mp) as f:
                m = json.load(f)
            if m.get("raw_sha256"):
                out.add(m["raw_sha256"])
        except (OSError, ValueError):
            pass
    return out


# --- codex ------------------------------------------------------------------
def codex_generate(slot, i, prompt, dry_run=False):
    wd = work_dir(slot)
    os.makedirs(wd, exist_ok=True)
    raw = os.path.join(wd, "raw_%d.png" % i)
    rraw = rel(raw)
    instr = ("Use your image generation tool to create ONE image: %s Save the "
             "resulting PNG into %s as %s (copy it from wherever the tool writes "
             "it). Then run: file %s."
             % (prompt, rel(wd), os.path.basename(raw), rraw))
    last = os.path.join(wd, "codex_%d_last.txt" % i)
    events = os.path.join(wd, "codex_%d_events.jsonl" % i)
    err = os.path.join(wd, "codex_%d_err.txt" % i)
    cmd = ["timeout", str(CODEX_TIMEOUT_S), "codex", "exec", "--sandbox",
           "workspace-write", "--json", "-o", rel(last), instr]
    if dry_run:
        print("DRY-RUN %s #%d\n  cmd: %s\n  prompt: %s" %
              (slot["path"], i, " ".join(cmd[:-1]) + " <instruction>", prompt))
        return None
    t0 = time.time()
    with open(events, "w") as fo, open(err, "w") as fe, open(os.devnull) as fi:
        rc = subprocess.call(cmd, cwd=REPO, stdin=fi, stdout=fo, stderr=fe)
    dur = time.time() - t0
    how = "copied-by-codex"
    if not _valid_png(raw):
        how = _recover(raw, [events, last], t0)
    meta = {"slot": slot["path"], "i": i, "prompt": prompt, "raw": rraw,
            "exit_code": rc, "duration_s": round(dur, 1), "raw_source": how}
    if how and _valid_png(raw):
        meta["raw_sha256"] = sha256(raw)
        dup = meta["raw_sha256"] in known_raw_hashes(exclude=_meta_path(slot, i))
        if dup:
            how = "DUPLICATE"
            os.remove(raw)
    ok = bool(how) and how != "DUPLICATE" and os.path.exists(raw)
    log("codex slot=%s i=%d rc=%d dur=%.1fs result=%s" %
        (slot["path"], i, rc, dur, how if ok else "FAILED(%s)" % how))
    if not ok:
        return None
    _write_meta(slot, i, meta)
    return meta


def _valid_png(p):
    try:
        with Image.open(p) as im:
            im.verify()
        return True
    except Exception:
        return False


def _recover(raw, texts, t0):
    """Find the generated PNG when Codex did not copy it into the workspace."""
    cands = []
    for t in texts:
        try:
            with open(t, errors="replace") as f:
                cands += re.findall(r"(/[^\s\"'\\]*generated_images/[^\s\"'\\]+?\.png)", f.read())
        except OSError:
            pass
    for c in reversed(cands):
        if os.path.exists(c) and os.path.getmtime(c) >= t0 - 1 and _valid_png(c):
            shutil.copyfile(c, raw)
            return "recovered-from-log:" + c
    newest = sorted(glob.glob(os.path.join(CODEX_IMG_DIR, "*", "*.png")),
                    key=os.path.getmtime)
    if newest and os.path.getmtime(newest[-1]) >= t0 - 1 and _valid_png(newest[-1]):
        shutil.copyfile(newest[-1], raw)
        return "recovered-newest:" + newest[-1]
    return None


def _meta_path(slot, i):
    return os.path.join(work_dir(slot), "%d.json" % i)


def _write_meta(slot, i, meta):
    with open(_meta_path(slot, i), "w") as f:
        json.dump(meta, f, indent=1)


def _read_meta(slot, i):
    p = _meta_path(slot, i)
    if not os.path.exists(p):
        return None
    with open(p) as f:
        return json.load(f)


def candidates(slot):
    out = []
    for p in glob.glob(os.path.join(work_dir(slot), "raw_*.png")):
        m = re.match(r"raw_(\d+)\.png$", os.path.basename(p))
        if m:
            out.append(int(m.group(1)))
    return sorted(out)


# --- image ops --------------------------------------------------------------
def _mean_diff(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3.0


def seam_metrics(im):
    """Wrap-edge vs interior adjacent-pixel difference (0-255, RGB mean)."""
    im = im.convert("RGB")
    w, h = im.size
    edge_h = _mean_diff(im.crop((0, 0, 1, h)), im.crop((w - 1, 0, w, h)))
    edge_v = _mean_diff(im.crop((0, 0, w, 1)), im.crop((0, h - 1, w, h)))
    int_h = _mean_diff(im.crop((0, 0, w - 1, h)), im.crop((1, 0, w, h)))
    int_v = _mean_diff(im.crop((0, 0, w, h - 1)), im.crop((0, 1, w, h)))
    excess = max(edge_h - int_h, edge_v - int_v)
    ratio = max(edge_h / max(int_h, 1e-6), edge_v / max(int_v, 1e-6))
    return {"edge_h": round(edge_h, 2), "edge_v": round(edge_v, 2),
            "interior_h": round(int_h, 2), "interior_v": round(int_v, 2),
            "excess": round(excess, 2), "ratio": round(ratio, 2),
            "flagged": excess > SEAM_MAX}


def roll(im, dx, dy):
    return ImageChops.offset(im, dx, dy)


def heal_seams(im):
    """Offset-by-half heal: blend the image with its half-offset copy.

    The half-offset copy is continuous across the wrap edge (its seam sits in
    the middle). A mask that is 0 at the borders and 1 inside a band of w/4
    takes the borders from the offset copy and the centre from the original,
    so neither seam survives.
    """
    w, h = im.size
    shifted = roll(im, w // 2, h // 2)
    bx, by = max(1, w // 4), max(1, h // 4)
    mask = Image.new("L", (w, h))
    px = mask.load()
    for y in range(h):
        ty = min(1.0, (min(y, h - 1 - y) + 0.5) / by)
        for x in range(w):
            tx = min(1.0, (min(x, w - 1 - x) + 0.5) / bx)
            t = min(tx, ty)
            px[x, y] = int(round(255 * t * t * (3 - 2 * t)))  # smoothstep
    return Image.composite(im, shifted, mask)


def quantize(im, colors=QUANT_COLORS):
    im = im.convert("RGB")
    pal = im.quantize(colors, method=Image.Quantize.MEDIANCUT)
    return im.quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)


def write_bmp8(p_img, path):
    """Uncompressed (BI_RGB) bottom-up 8-bit BMP with a full 256-entry palette."""
    assert p_img.mode == "P"
    w, h = p_img.size
    pal = (p_img.getpalette() or [])[:768]
    pal += [0] * (768 - len(pal))
    stride = (w + 3) & ~3
    off = 14 + 40 + 1024
    size = off + stride * h
    out = bytearray()
    out += struct.pack("<2sIHHI", b"BM", size, 0, 0, off)
    out += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, stride * h,
                       2835, 2835, 256, 0)
    for k in range(256):
        r, g, b = pal[3 * k:3 * k + 3]
        out += bytes((b, g, r, 0))
    data = p_img.tobytes()
    pad = b"\0" * (stride - w)
    for y in range(h - 1, -1, -1):
        out += data[y * w:(y + 1) * w] + pad
    with open(path, "wb") as f:
        f.write(out)


def verify_bmp8(path, size=None):
    """Return list of problems (empty == OK)."""
    probs = []
    with open(path, "rb") as f:
        b = f.read()
    if b[:2] != b"BM":
        return ["not BM"]
    fsize, off = struct.unpack_from("<I", b, 2)[0], struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    comp = struct.unpack_from("<I", b, 30)[0]
    if bpp != 8:
        probs.append("bpp=%d" % bpp)
    if comp != 0:
        probs.append("biCompression=%d" % comp)
    if fsize != len(b):
        probs.append("bfSize %d != len %d" % (fsize, len(b)))
    exp = 14 + 40 + 1024 + ((w + 3) & ~3) * abs(h)
    if len(b) != exp or off != 1078:
        probs.append("size %d != %d (off %d)" % (len(b), exp, off))
    if size and (w, abs(h)) != tuple(size):
        probs.append("dims %dx%d != %dx%d" % (w, abs(h), size[0], size[1]))
    return probs


def tile2x2(im):
    w, h = im.size
    out = Image.new("RGB", (2 * w, 2 * h))
    for dx in (0, w):
        for dy in (0, h):
            out.paste(im.convert("RGB"), (dx, dy))
    return out


def postprocess(slot, i):
    wd = work_dir(slot)
    raw = os.path.join(wd, "raw_%d.png" % i)
    meta = _read_meta(slot, i) or {"slot": slot["path"], "i": i,
                                   "prompt": build_prompt(slot),
                                   "raw": rel(raw), "raw_sha256": sha256(raw)}
    if slot.get("ref_format") in ("dds_argb8", "dds_argb4"):
        return postprocess_dds(slot, i, raw, meta)
    if slot.get("ref_format", "bmp8") != "bmp8":
        sys.exit("artgen: ref_format %s not implemented yet" % slot.get("ref_format"))
    if slot.get("alpha", "none") != "none":
        sys.exit("artgen: alpha %s not implemented yet" % slot.get("alpha"))
    size = tuple(slot.get("ref_size", (256, 256)))
    with Image.open(raw) as src:
        meta["raw_size"] = list(src.size)
        im = src.convert("RGB").resize(size, Image.Resampling.LANCZOS)
    healed = False
    if slot.get("tileable"):
        s0 = seam_metrics(im)
        meta["seam_raw"] = s0
        # seam_heal:false for framed designs (crate faces, panels) whose
        # border is meant to repeat; healing would ghost the frame.
        if s0["flagged"] and slot.get("seam_heal", True):
            im = heal_seams(im)
            healed = True
    q = quantize(im)
    bmp = os.path.join(wd, "%d.bmp" % i)
    write_bmp8(q, bmp)
    probs = verify_bmp8(bmp, size)
    if probs:
        sys.exit("artgen: BMP verification failed for %s: %s" % (bmp, probs))
    final = Image.open(bmp).convert("RGB")
    if slot.get("tileable"):
        meta["seam"] = seam_metrics(final)
        meta["seam"]["healed"] = healed
    final.save(os.path.join(wd, "%d_256.png" % i))
    tile2x2(final).save(os.path.join(wd, "%d_tiled.png" % i))
    meta.update({"bmp": rel(bmp), "bmp_sha256": sha256(bmp),
                 "bmp_bytes": os.path.getsize(bmp)})
    _write_meta(slot, i, meta)
    s = meta.get("seam", {})
    log("post slot=%s i=%d bmp=%dB seam_raw_excess=%s seam_final_excess=%s healed=%s" %
        (slot["path"], i, meta["bmp_bytes"], meta.get("seam_raw", {}).get("excess"),
         s.get("excess"), healed))
    return meta


def _label(draw, xy, text):
    x, y = xy
    draw.rectangle((x, y, x + 6 * len(text) + 4, y + 12), fill=(0, 0, 0))
    draw.text((x + 2, y + 1), text, fill=(255, 255, 255))


def make_sheet(slot, thumb=256):
    wd = work_dir(slot)
    idx = [i for i in candidates(slot) if os.path.exists(os.path.join(wd, "%d_tiled.png" % i))]
    if not idx:
        return None
    sheet = Image.new("RGB", (len(idx) * (thumb + 8) + 8, thumb + 16), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for k, i in enumerate(idx):
        with Image.open(os.path.join(wd, "%d_tiled.png" % i)) as candidate:
            t = candidate.resize((thumb, thumb), Image.Resampling.LANCZOS)
        x = 8 + k * (thumb + 8)
        # Preserve alpha in candidate previews by compositing over a checkerboard.
        if "A" in t.getbands():
            checker = Image.new("RGB", t.size)
            checker.putdata([(190, 190, 190) if (px // 16 + py // 16) % 2 else (235, 235, 235)
                             for py in range(t.height) for px in range(t.width)])
            sheet.paste(Image.alpha_composite(checker.convert("RGBA"), t).convert("RGB"), (x, 8))
        else:
            sheet.paste(t, (x, 8))
        m = _read_meta(slot, i) or {}
        s = m.get("seam", {})
        _label(d, (x, 8), "#%d seam %+.1f%s" % (i, s.get("excess", 0), " H" if s.get("healed") else ""))
    out = os.path.join(wd, "sheet.png")
    sheet.save(out)
    return out


# --- provenance ---------------------------------------------------------------
def read_provenance():
    if not os.path.exists(PROVENANCE):
        return []
    with open(PROVENANCE) as f:
        return [json.loads(l) for l in f if l.strip()]


def pick(slot, i):
    m = _read_meta(slot, i)
    ext = "dds" if slot.get("ref_format", "").startswith("dds_") else "bmp"
    bmp = os.path.join(work_dir(slot), "%d.%s" % (i, ext))
    if not m or not os.path.exists(bmp):
        sys.exit("artgen: candidate %d of %s not post-processed" % (i, slot["path"]))
    probs = verify_texture(bmp, slot)
    if probs:
        sys.exit("artgen: refusing to pick %s: %s" % (bmp, probs))
    dst = os.path.join(PACK_ROOT, slot["path"])
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(bmp, dst)
    digest = sha256(dst)
    rec = {"path": slot["path"], "sha256": digest, "generator": GENERATOR,
           "prompt": m["prompt"], "license": LICENSE, "author": AUTHOR,
           "date": dt.date.today().isoformat(), "curated_by": None,
           "seam_metric": dict(m["seam"], raw_excess=m.get("seam_raw", {}).get("excess"))
           if m.get("seam") else None, "candidate": i,
           "source_png_sha256": m.get("raw_sha256")}
    recs = [r for r in read_provenance() if r["path"] != slot["path"]] + [rec]
    with open(PROVENANCE, "w") as f:
        for r in recs:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    if os.path.exists(MANIFEST):  # pick => status curated (spec section 5)
        with open(MANIFEST) as f:
            man = json.load(f)
        hit = False
        for s in man.get("slots", []):
            if s.get("path") == slot["path"] and s.get("status") in ("missing", "generated"):
                s["status"] = "curated"
                hit = True
        if hit:
            with open(MANIFEST, "w") as f:
                json.dump(man, f, indent=2)
                f.write("\n")
    log("pick slot=%s i=%d -> %s sha256=%s" % (slot["path"], i, rel(dst), digest[:12]))


# --- commands -----------------------------------------------------------------
def cmd_gen(a):
    slots = select(a.target)
    total = 0.0
    for slot in slots:
        missing = [f for f in SLOT_FIELDS if f not in slot]
        if missing:
            sys.exit("artgen: slot %s lacks %s" % (slot.get("path"), missing))
        prompt = build_prompt(slot)
        for i in range(1, a.n + 1):
            raw = os.path.join(work_dir(slot), "raw_%d.png" % i)
            if os.path.exists(raw):
                if a.dry_run:
                    print("skip %s #%d (exists)" % (slot["path"], i))
                    continue
            else:
                m = codex_generate(slot, i, prompt, a.dry_run)
                if a.dry_run:
                    continue
                if m:
                    total += m["duration_s"]
                if not m:
                    continue
            m = _read_meta(slot, i)
            if a.force_post or not m or not any(k in m for k in ("bmp_sha256", "dds_sha256")):
                postprocess(slot, i)
        if not a.dry_run:
            make_sheet(slot)
    if not a.dry_run:
        log("gen done target=%s codex_time=%.1fs" % (a.target, total))


def cmd_pick(a):
    pick(resolve_slot(a.slot), a.i)


def cmd_sheet(a):
    print(make_sheet(resolve_slot(a.slot)) or "no candidates")


def cmd_status(a):
    slots = select(a.batch) if a.batch else list(all_known_slots().values())
    prov = {r["path"]: r for r in read_provenance()}
    print("%-40s %-6s %-36s %s" % ("slot", "cands", "seam excess (final) per cand", "picked"))
    for s in slots:
        c = candidates(s)
        seams = []
        for i in c:
            m = _read_meta(s, i) or {}
            e = m.get("seam", {}).get("excess")
            seams.append("#%d:%s%s" % (i, "-" if e is None else "%.1f" % e,
                                       "!" if m.get("seam", {}).get("flagged") else ""))
        dst = os.path.join(PACK_ROOT, s["path"])
        r = prov.get(s["path"])
        if r and os.path.exists(dst):
            ok = sha256(dst) == r["sha256"] and not verify_texture(dst, s)
            picked = "#%s %s" % (r.get("candidate"), "ok" if ok else "MISMATCH")
        else:
            picked = "-"
        print("%-40s %-6d %-36s %s" % (s["path"], len(c), " ".join(seams), picked))


def cmd_preview(a):
    slots = load_batch(a.batch)
    th = a.thumb
    cols = 4
    rows = (len(slots) + cols - 1) // cols
    pad = 8
    sheet = Image.new("RGB", (cols * (th + pad) + pad, rows * (th + pad + 14) + pad), (32, 32, 32))
    d = ImageDraw.Draw(sheet)
    for k, s in enumerate(slots):
        p = os.path.join(PACK_ROOT, s["path"])
        x = pad + (k % cols) * (th + pad)
        y = pad + (k // cols) * (th + pad + 14)
        if os.path.exists(p):
            t = tile2x2(Image.open(p)).resize((th, th), Image.Resampling.LANCZOS)
            sheet.paste(t, (x, y + 14))
        d.text((x, y + 1), os.path.basename(s["path"]), fill=(230, 230, 230))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    sheet.save(a.out, optimize=True)
    print(a.out)


def cmd_verify(a):
    bad = 0
    for p in a.files:
        probs = verify_dds(p) if p.lower().endswith(".dds") else verify_bmp8(p)
        print("%s %s (%d bytes)" % ("OK  " if not probs else "FAIL", p, os.path.getsize(p)),
              probs or "")
        bad += bool(probs)
    sys.exit(1 if bad else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    g = sp.add_parser("gen")
    g.add_argument("target")
    g.add_argument("--n", type=int, default=ARTGEN_CANDIDATES)
    g.add_argument("--dry-run", action="store_true")
    g.add_argument("--force-post", action="store_true", help="re-run post-processing on existing raws")
    g.set_defaults(fn=cmd_gen)
    p = sp.add_parser("pick")
    p.add_argument("slot")
    p.add_argument("i", type=int)
    p.set_defaults(fn=cmd_pick)
    s = sp.add_parser("sheet")
    s.add_argument("slot")
    s.set_defaults(fn=cmd_sheet)
    st = sp.add_parser("status")
    st.add_argument("batch", nargs="?")
    st.set_defaults(fn=cmd_status)
    pv = sp.add_parser("preview")
    pv.add_argument("batch")
    pv.add_argument("out")
    pv.add_argument("--thumb", type=int, default=192)
    pv.set_defaults(fn=cmd_preview)
    v = sp.add_parser("verify")
    v.add_argument("files", nargs="+")
    v.set_defaults(fn=cmd_verify)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()

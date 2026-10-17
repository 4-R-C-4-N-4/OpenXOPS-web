#!/usr/bin/env python3
"""Generate assets/manifest.json: every data file (slot) the engine needs.

Inputs
  src/*.cpp, src/*.h                     upstream OpenXOPS 1.080, CP932 + CRLF
  tools/manifest/xops096_reference.json  factual XOPS 0.96 metadata (size, md5, shape)
  assets/manifest.json (if present)      only to carry over the manual `status` field
  assets/pack/data/map*/temp.bd1 (opt.)  the pack's own maps define the map_tile slots

The output is deterministic: slots are sorted by lower-case path, JSON keys are
sorted, and nothing depends on time, locale or dict ordering. Run it twice and
`git diff --exit-code assets/manifest.json` stays clean.

Usage: python3 tools/manifest/build.py [--check] [--quiet]
  --check   do not write; exit 1 if the manifest on disk is out of date
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

UPSTREAM_REF = "upstream-1.080"
from collections import Counter, OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "src")
REF_PATH = os.path.join(HERE, "xops096_reference.json")
OUT_PATH = os.path.join(ROOT, "assets", "manifest.json")
PACK_ROOT = os.path.join(ROOT, "assets", "pack")

BD1_TEXTURES = 10      # TOTAL_BLOCKTEXTURE (datafile.h)
BD1_NAME_LEN = 31      # fread(texture[i], 1, 31, fp) (datafile.cpp)

# --------------------------------------------------------------------------
# Source reading
# --------------------------------------------------------------------------


class Source:
    """All upstream source lines (decoded CP932), with comment-aware literal scan."""

    def __init__(self, src_dir):
        # The manifest describes the data contract of pristine upstream, so read the
        # pinned tag (CP932 + CRLF) rather than the working tree, which the port edits
        # and which is UTF-8 after Phase 0. Citations then refer to upstream-1.080
        # line numbers. Fall back to the working tree only when the tag is absent.
        self.files = OrderedDict()
        repo = os.path.dirname(os.path.abspath(src_dir))
        try:
            ls = subprocess.run(["git", "-C", repo, "ls-tree", "--name-only", UPSTREAM_REF + ":src"],
                                check=True, capture_output=True).stdout.decode().split()
            names = sorted(n for n in ls if n.endswith((".cpp", ".h")))
            for n in names:
                raw = subprocess.run(["git", "-C", repo, "show", "%s:src/%s" % (UPSTREAM_REF, n)],
                                     check=True, capture_output=True).stdout
                self.files[n] = raw.decode("cp932").splitlines()
        except (OSError, subprocess.CalledProcessError):
            names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(src_dir, "*.cpp"))
                           + glob.glob(os.path.join(src_dir, "*.h")))
            for n in names:
                with open(os.path.join(src_dir, n), "rb") as f:
                    raw = f.read()
                try:
                    txt = raw.decode("utf-8")
                except UnicodeDecodeError:
                    txt = raw.decode("cp932")
                self.files[n] = txt.splitlines()
        self.literals = []   # (file, lineno, value, code_text)
        for n, lines in self.files.items():
            self._scan(n, lines)

    def _scan(self, name, lines):
        in_block = False
        for no, line in enumerate(lines, 1):
            code, lits, in_block = _split_code(line, in_block)
            for v in lits:
                self.literals.append((name, no, v, code))

    def code_lines(self, name):
        """Yield (lineno, code_without_comments, comment_text) for a file."""
        in_block = False
        for no, line in enumerate(self.files[name], 1):
            code, _lits, in_block2 = _split_code(line, in_block)
            comment = ""
            s = line.strip()
            if s.startswith("//") and not in_block:
                comment = s[2:].strip()
            in_block = in_block2
            yield no, code, comment

    def find(self, name, needle, start=1):
        """Return 'src/<name>:<line>' of the first code line containing needle."""
        for no, code, _c in self.code_lines(name):
            if no >= start and needle in code:
                return "src/%s:%d" % (name, no)
        raise SystemExit("build.py: source anchor not found: %s in %s" % (needle, name))


def _split_code(line, in_block):
    """Strip // and /* */ comments; return (code, [string literal values], in_block)."""
    out, lits, i, n = [], [], 0, len(line)
    while i < n:
        if in_block:
            j = line.find("*/", i)
            if j < 0:
                return "".join(out), lits, True
            i, in_block = j + 2, False
            continue
        c = line[i]
        if c == "/" and line.startswith("//", i):
            break
        if c == "/" and line.startswith("/*", i):
            in_block, i = True, i + 2
            continue
        if c == "'":
            j = i + 1
            while j < n and line[j] != "'":
                j += 2 if line[j] == "\\" else 1
            out.append(line[i:j + 1])
            i = j + 1
            continue
        if c == '"':
            j, buf = i + 1, []
            while j < n and line[j] != '"':
                if line[j] == "\\" and j + 1 < n:
                    buf.append(line[j + 1])
                    j += 2
                else:
                    buf.append(line[j])
                    j += 1
            lits.append("".join(buf))
            out.append(line[i:j + 1])
            i = j + 1
            continue
        out.append(c)
        i += 1
    return "".join(out), lits, in_block


DATA_RE = re.compile(r"^(?:\./)?data[\\/]", re.I)


def norm(p):
    p = p.replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    while "//" in p:
        p = p.replace("//", "/")
    return p


def literal_category(p):
    parts = p.lower().split("/")
    if len(parts) == 2:
        return "root"
    seg = parts[1]
    if re.match(r"map\d+$", seg):
        return "map"
    return seg


# --------------------------------------------------------------------------
# Parameter tables (parameter.cpp)
# --------------------------------------------------------------------------

def _cval(expr):
    expr = expr.strip().rstrip(";").strip()
    m = re.match(r'^"(.*)"$', expr)
    if m:
        return m.group(1).replace("\\\\", "\\")
    if re.match(r"^-?\d+$", expr):
        return int(expr)
    if expr in ("true", "false"):
        return expr == "true"
    return expr


def parse_tables(src):
    """Return dict of parameter tables, each entry carrying field -> (value, line)."""
    t = {"Human": {}, "Weapon": {}, "SmallObject": {}, "Bullet": {}, "MissionData": {},
         "HumanTexturePath": {}}
    rx = re.compile(r"(?<![A-Za-z_])(Human|Weapon|SmallObject|Bullet|MissionData)\[(\d+)\]\.(\w+)(?:\[(\d+)\])?\s*=\s*(.+)$")
    rxp = re.compile(r"(?<![A-Za-z_])HumanTexturePath\[(\d+)\]\s*=\s*(.+)$")
    last_comment = None
    for no, code, comment in src.code_lines("parameter.cpp"):
        if comment:
            last_comment = comment
            continue
        m = rx.search(code)
        if m:
            tbl, idx, field, sub, val = m.group(1), int(m.group(2)), m.group(3), m.group(4), m.group(5)
            e = t[tbl].setdefault(idx, {"_comment": last_comment, "_line": no})
            key = field + (sub or "")
            e[key] = (_cval(val), no)
            continue
        m = rxp.search(code)
        if m:
            t["HumanTexturePath"][int(m.group(1))] = (norm(_cval(m.group(2))), no)
    return t


# Our own English glosses for the Japanese comments in parameter.cpp.
GLOSS = [
    ("戦闘用ロボット", "combat robot"), ("特殊", "special forces"), ("スーツ", "man in suit"),
    ("警察官", "police officer"), ("シャツ男", "man in shirt"), ("中東兵", "desert militia fighter"),
    ("金髪男", "blond man"), ("市民", "civilian"), ("兵士", "soldier"), ("ゾンビ", "zombie"),
    ("将軍", "general in uniform"), ("ゲイツ", "VIP businessman"), ("ハゲ", "bald man"),
    ("女", "woman"),
    ("黒", "black"), ("緑", "green"), ("白", "white"), ("紫", "violet"), ("青", "blue"),
    ("紺", "navy"), ("灰", "grey"), ("茶", "brown"),
    ("パソコン", "desktop PC"), ("キーボード上", "keyboard on top"), ("本体逆", "tower on the other side"),
    ("ワイド", "widescreen"), ("起動中", "powered on"), ("暗", "dark screen"), ("缶", "drink can"),
    ("椅子", "chair"), ("ダンボール", "cardboard box"), ("パイロン", "traffic cone"),
]


def gloss(text):
    if not text:
        return ""
    words = []
    for tok in text.split():
        if tok == "SG":
            continue        # variant marker in the preset table; not visual
        for jp, en in GLOSS:
            tok = tok.replace(jp, " " + en + " ")
        words.append(" ".join(tok.split()))
    return " ".join(w for w in words if w)


# --------------------------------------------------------------------------
# Reference metadata
# --------------------------------------------------------------------------

def load_reference():
    with open(REF_PATH, encoding="utf-8") as f:
        doc = json.load(f)
    return {e["path"].lower(): e for e in doc["files"]}


# Fallback shapes when the reference list has no row (F-4 in FREE-DATA-PACK.md).
DEFAULT_SHAPE = {
    "skin": ([256, 256], "bmp8"),
    "weapon_skin": ([128, 128], "bmp8"),
    "article_skin": ([128, 128], "bmp8"),
    "sky": ([256, 256], "bmp8"),
    "briefing": ([128, 128], "bmp8"),
    "map_tile": ([128, 128], "bmp8"),
    "effect": ([128, 128], "dds_argb8"),
}


# --------------------------------------------------------------------------
# Slot assembly
# --------------------------------------------------------------------------

class Slots:
    def __init__(self):
        self.by_key = {}

    def add(self, path, **kw):
        path = norm(path)
        key = path.lower()
        refs = kw.pop("source_refs", [])
        s = self.by_key.get(key)
        if s is None:
            s = {"path": path, "source_refs": []}
            self.by_key[key] = s
            for k, v in kw.items():
                s[k] = v
        else:
            for k, v in kw.items():
                if k == "hard":
                    s[k] = s.get(k, False) or v
                elif k == "priority":
                    s[k] = min(s.get(k, v), v)       # "P0" < "P1" < "P2"
                elif k == "note" and s.get("note") and v and v not in s["note"]:
                    s["note"] = s["note"] + " " + v
                elif s.get(k) in (None, "") and v not in (None, ""):
                    s[k] = v
        for r in refs:
            if r not in s["source_refs"]:
                s["source_refs"].append(r)
        return s


def kind_of(path):
    low = path.lower()
    if low.endswith("/"):
        return "dir"
    ext = os.path.splitext(low)[1]
    return {".bmp": "texture", ".dds": "texture", ".png": "texture", ".jpg": "texture",
            ".x": "model", ".wav": "sound", ".bd1": "map", ".pd1": "points",
            ".txt": "mission", ".msg": "mission"}.get(ext, "other")


def parse_bd1_textures(path):
    """Return the non-empty texture names from a .bd1 header (10 x 31 bytes)."""
    with open(path, "rb") as f:
        head = f.read(BD1_TEXTURES * BD1_NAME_LEN)
    names = []
    for i in range(BD1_TEXTURES):
        raw = head[i * BD1_NAME_LEN:(i + 1) * BD1_NAME_LEN].split(b"\0", 1)[0]
        name = raw.decode("cp932", "replace").strip()
        names.append(name)
    return names


def find_ci(root, rel):
    """Case-insensitive lookup of rel (forward slashes) under root; returns real path or None."""
    cur = root
    for part in rel.split("/"):
        if not part:
            continue
        try:
            entries = os.listdir(cur)
        except OSError:
            return None
        hit = sorted(e for e in entries if e.lower() == part.lower())
        if not hit:
            return None
        cur = os.path.join(cur, hit[0])
    return cur


def build(quiet=False):
    src = Source(SRC)
    ref = load_reference()
    tbl = parse_tables(src)
    log = []

    # ---- 1. every distinct "data..." string literal -------------------------
    lit_refs = OrderedDict()
    lit_ctx = {}
    for fname, no, val, code in src.literals:
        if not DATA_RE.match(val):
            continue
        p = norm(val)
        key = p.lower()
        lit_refs.setdefault(key, (p, []))[1].append("src/%s:%d" % (fname, no))
        lit_ctx.setdefault(key, (fname, no, code))
    lit_counts = Counter(literal_category(p) for p, _r in lit_refs.values())

    slots = Slots()
    patterns = []

    # ---- tables -------------------------------------------------------------
    humans = tbl["Human"]
    weapons = tbl["Weapon"]
    smallobj = tbl["SmallObject"]
    missions = tbl["MissionData"]
    htex = tbl["HumanTexturePath"]

    def hv(e, k, d=None):
        return e[k][0] if k in e else d

    def wname(i):
        return str(hv(weapons[i], "name", "weapon %d" % i)).strip()

    # texture id -> list of human presets using it; up model index -> presets
    tex_users, up_users = {}, {}
    for i in sorted(humans):
        e = humans[i]
        tex_users.setdefault(hv(e, "texture"), []).append(i)
        up_users.setdefault(hv(e, "model"), []).append(i)

    # ---- priority policy ----------------------------------------------------
    # P0 humans: special forces green A (most common preset family) and soldier 1 A.
    P0_HUMANS = [2, 26]
    p0_tex = {hv(humans[i], "texture") for i in P0_HUMANS}
    p0_weapons = set()
    for i in P0_HUMANS:
        for k in ("Weapon0", "Weapon1"):
            if hv(humans[i], k):
                p0_weapons.add(hv(humans[i], k))
    # Bullet[1] (the thrown grenade object) reuses the grenade weapon model/texture.
    grenade_ids = [i for i in weapons if str(hv(weapons[i], "name", "")).strip() == "GRENADE"]
    p0_weapons.update(grenade_ids)
    p0_weapon_paths = set()
    for i in p0_weapons:
        for k in ("model", "texture"):
            v = hv(weapons[i], k)
            if v:
                p0_weapon_paths.add(norm(v).lower())

    # ---- 2. hard literal slots ---------------------------------------------
    sky_pattern_key = None
    for key, (p, refs) in lit_refs.items():
        fname, no, code = lit_ctx[key]
        if "%" in p:
            sky_pattern_key = key
            continue                                      # expanded below
        kind = kind_of(p)
        cat = literal_category(p)
        base = os.path.basename(p.rstrip("/"))
        stem = os.path.splitext(base)[0]
        cls, summary, prio, uv, note = "other", "", "P1", None, None

        if kind == "dir":
            if cat == "briefing":
                cls = "briefing"
                summary = "Briefing picture directory; mission scripts name the pictures inside it."
                prio = "P0"
                note = ("Dynamic: datafile.cpp builds data/briefing/<name>.bmp from lines 1-2 of a "
                        "mission .txt (\"!\" = no picture). The pack's own mission scripts define the names.")
            else:
                cls = "other"
                summary = "Map directory %s (block map, point files, mission scripts, tile textures)." % base
                prio = "P2"
                note = "Holds the pack's own temp.bd1 and the tile textures its header names."
        elif cat == "root":
            if stem == "char":
                cls, prio = "hud", "P0"
                summary = "Font and HUD atlas, loaded once at D3D/GL init (InitD3D TextureFontFname)."
            elif stem == "title":
                cls, prio = "hud", "P0"
                summary = "Title logo drawn over the menu demo and briefing screens (gametitle)."
            elif stem == "scope":
                cls, prio = "effect", "P0"
                summary = "Scope overlay drawn full-screen when zoomed (scopetexture)."
            else:
                cls, prio = "effect", "P0"
                what = {"blood": "blood splash sprite", "mflash": "muzzle flash sprite",
                        "smoke": "smoke puff sprite", "yakkyou": "ejected cartridge case sprite"}
                summary = "Effect billboard: %s (effecttexture_%s)." % (what.get(stem, stem), stem)
        elif cat == "sound":
            cls, prio = "sfx", "P0"
            summary = sound_summary(stem, code, weapons, smallobj, hv, wname)
        elif cat == "sky":
            cls, prio = "model", "P0"
            summary = "Sky box mesh; every sky texture wraps this model."
        elif cat == "article":
            if kind == "model":
                cls = "model"
                users = [i for i in sorted(smallobj) if norm(hv(smallobj[i], "model", "")).lower() == key]
                summary = "Small object model: %s." % "; ".join(gloss(smallobj[i]["_comment"]) for i in users)
            else:
                cls = "article_skin"
                users = [i for i in sorted(smallobj) if norm(hv(smallobj[i], "texture", "")).lower() == key]
                summary = "Small object skin: %s." % "; ".join(gloss(smallobj[i]["_comment"]) for i in users)
                uv = norm(hv(smallobj[users[0]], "model")) if users else None
            prio = "P1"
        elif cat == "model":
            if "/weapon/" in key:
                ws = [i for i in sorted(weapons) for k in ("model", "texture")
                      if norm(hv(weapons[i], k, "") or "").lower() == key]
                names = sorted({wname(i) for i in ws})
                if kind == "model":
                    cls = "model"
                    summary = "Weapon model: %s (held in hand and dropped on the ground)." % ", ".join(names)
                else:
                    cls = "weapon_skin"
                    summary = "Weapon skin: %s." % ", ".join(names)
                    uv = norm(hv(weapons[ws[0]], "model")) if ws else None
                prio = "P0" if key in p0_weapon_paths else "P1"
            elif stem == "bullet":
                cls = "model" if kind == "model" else "weapon_skin"
                summary = "Bullet object %s (Bullet[0]); drawn for every shot in flight." % (
                    "model" if kind == "model" else "skin")
                uv = "data/model/bullet.x" if kind == "texture" else None
                prio = "P0"
            elif kind == "model":
                cls, prio = "model", "P0"
                summary = human_model_summary(stem, humans, up_users, weapons, hv, wname)
            else:
                cls = "skin"
                tid = next((i for i, (pp, _n) in htex.items() if pp.lower() == key), None)
                users = tex_users.get(tid, [])
                descs = sorted({gloss(humans[i]["_comment"]) for i in users})
                ups = sorted(u for u in {hv(humans[i], "model") for i in users} if isinstance(u, int))
                summary = "Character skin: %s." % ("; ".join(descs) if descs else "unused by the preset table")
                if ups:
                    uv = "data/model/up%d.x" % ups[0]
                    if len(ups) > 1:
                        note = "Also worn on body models %s." % ", ".join("up%d.x" % u for u in ups[1:])
                if tid is not None:
                    summary = summary[:-1] + " (HumanTexturePath[%d])." % tid
                prio = "P0" if tid in p0_tex else "P1"
        elif cat == "map":
            if kind == "map":
                cls, prio = "other", "P0"
                summary = "Block map for the opening fly-through (map10)."
            elif kind == "points":
                cls, prio = "other", "P0"
                summary = "Point file for the opening fly-through: camera path and actors."
        slots.add(p, kind=kind, **{"class": cls}, hard=True, source_refs=refs, summary=summary,
                  priority=prio, uv_layout=uv, note=note)

    # ---- 3. sky%d pattern ---------------------------------------------------
    if sky_pattern_key:
        p, refs = lit_refs[sky_pattern_key]
        range_ref = src.find("resource.cpp", "(5 < id)")
        opening_ref = src.find("gamemain.cpp", "LoadSkyModelTexture(1)")
        patterns.append({"pattern": p, "expands_to": [p.replace("%d", str(i)) for i in range(1, 6)],
                         "source_refs": refs + [range_ref],
                         "note": "sprintf with id 1..5 (LoadSkyModelTexture range check); "
                                 "the mission script's 3rd line picks the number."})
        for i in range(1, 6):
            sp = p.replace("%d", str(i))
            slots.add(sp, kind="texture", **{"class": "sky"}, hard=True,
                      source_refs=refs + [range_ref] + ([opening_ref] if i == 1 else []),
                      summary="Sky texture %d of 5, wraps the sky box mesh.%s" % (
                          i, " Used by the opening and menu." if i == 1 else ""),
                      priority="P0" if i == 1 else "P1", uv_layout="data/sky/sky.x",
                      note="Expanded from sprintf pattern %s." % p)

    # ---- 4. map and mission files built at run time -------------------------
    bd1_ref = src.find("parameter.h", "OFFICIALMISSION_BD1")
    bd1_use = src.find("gamemain.cpp", "strcat(bdata, OFFICIALMISSION_BD1)")
    pd1_use = src.find("gamemain.cpp", 'strcat(pdata, ".pd1")')
    txt_use = src.find("gamemain.cpp", 'strcat(path, ".txt")')
    msg_use = src.find("datafile.cpp", 'strcat(fname2, ".msg")')
    demo_bd1 = src.find("gamemain.cpp", 'strcat(bdata, "temp.bd1")')
    demo_pd1 = src.find("gamemain.cpp", 'strcat(pdata, "demo.pd1")')
    maptex_use = src.find("d3dgraphics-opengl.cpp", "mapTextureID[i] = LoadTexture(fnamefull")
    brief_use = src.find("datafile.cpp", 'strcpy(picturefileA_path, "data\\\\briefing\\\\")')
    patterns += [
        {"pattern": "<MissionData.directory>temp.bd1", "source_refs": [bd1_ref, bd1_use],
         "note": "Standard mission block map: directory + OFFICIALMISSION_BD1."},
        {"pattern": "<MissionData.directory><MissionData.txt>.pd1", "source_refs": [pd1_use],
         "note": "Standard mission point file."},
        {"pattern": "<MissionData.directory><MissionData.txt>.txt", "source_refs": [txt_use],
         "note": "Standard mission briefing script (picture names, sky number, briefing text)."},
        {"pattern": "<point file without extension>.msg", "source_refs": [msg_use],
         "note": "Optional event message file next to every .pd1; load failure is ignored."},
        {"pattern": "<demopath>temp.bd1 and <demopath>demo.pd1", "source_refs": [demo_bd1, demo_pd1],
         "note": "Menu background demo; demopath is one of 6 map dirs chosen by GetRand(6). "
                 "Any missing file shows 'block/point data open failed', so all six are P0."},
        {"pattern": "<map dir><bd1 texture name>", "source_refs": [maptex_use],
         "note": "Map tiles: 10 x 31-byte names in the .bd1 header, relative to the .bd1 dir. "
                 "Defined by the pack's own maps; listed as map_tile slots once those .bd1 files exist."},
        {"pattern": "data/briefing/<name>.bmp", "source_refs": [brief_use],
         "note": "Briefing pictures named by mission scripts; covered by the data/briefing/ dir slot."},
    ]

    # Demo maps (menu background). Source order of the switch gives the dir list.
    demo_dirs = []
    for no, code, _c in src.code_lines("gamemain.cpp"):
        m = re.search(r'strcpy\(demopath,\s*"([^"]+)"\)', code)
        if m:
            demo_dirs.append((norm(m.group(1).replace("\\\\", "\\")), "src/gamemain.cpp:%d" % no))
    for d, r in demo_dirs:
        mapname = map_name(d, missions, hv)
        slots.add(d + "temp.bd1", kind="map", **{"class": "other"}, hard=False,
                  source_refs=[r, demo_bd1], priority="P0",
                  summary="Block map for %s (%s); also the menu background demo." % (d.rstrip("/"), mapname))
        slots.add(d + "demo.pd1", kind="points", **{"class": "other"}, hard=False,
                  source_refs=[r, demo_pd1], priority="P0",
                  summary="Point file for the menu background demo on %s (%s)." % (d.rstrip("/"), mapname))

    # The opening (map10) .msg sits next to op.pd1.
    slots.add("data/map10/op.msg", kind="mission", **{"class": "other"}, hard=False,
              source_refs=[src.find("gamemain.cpp", 'op.pd1'), msg_use], priority="P1",
              summary="Optional event messages for the opening fly-through.",
              note="Optional: a missing .msg is ignored.")

    # Standard missions. P0 mission: ALLEY_EXT reuses map10/temp.bd1, already P0 for the opening.
    P0_MISSION = "ALLEY_EXT"
    for idx in sorted(missions):
        e = missions[idx]
        d = norm(hv(e, "directory"))
        txt = hv(e, "txt")
        name = str(hv(e, "name")).strip()
        full = str(hv(e, "fullname")).strip()
        dref = "src/parameter.cpp:%d" % e["directory"][1]
        tref = "src/parameter.cpp:%d" % e["txt"][1]
        p0 = name == P0_MISSION
        prio = "P0" if p0 else "P2"
        mapname = map_name(d, missions, hv)
        slots.add(d + "temp.bd1", kind="map", **{"class": "other"}, hard=False,
                  source_refs=[dref, bd1_use], priority=prio,
                  summary="Block map for %s (%s)." % (d.rstrip("/"), mapname))
        slots.add(d + txt + ".pd1", kind="points", **{"class": "other"}, hard=False,
                  source_refs=[dref, tref, pd1_use], priority=prio,
                  summary="Point file for standard mission %d '%s' (%s): spawns, paths, events." % (idx, name, full))
        slots.add(d + txt + ".txt", kind="mission", **{"class": "other"}, hard=False,
                  source_refs=[dref, tref, txt_use], priority=prio,
                  summary="Briefing script for standard mission %d '%s' (%s)." % (idx, name, full))
        slots.add(d + txt + ".msg", kind="mission", **{"class": "other"}, hard=False,
                  source_refs=[dref, tref, msg_use], priority="P1" if p0 else "P2",
                  summary="Event messages for standard mission %d '%s'." % (idx, name),
                  note="Optional: a missing .msg is ignored.")

    # Map dirs get the map name in their summary.
    for s in slots.by_key.values():
        if s["kind"] == "dir" and re.match(r"data/map\d+/$", s["path"].lower()):
            s["summary"] = "Map directory %s (%s): block map, point files, mission scripts, tiles." % (
                s["path"].rstrip("/"), map_name(s["path"], missions, hv))
            if s["path"].lower() in {d.lower() for d, _r in demo_dirs} | {"data/map10/"}:
                s["priority"] = "P0"

    # ---- 5. map tiles from the pack's own .bd1 files ------------------------
    for s in sorted(list(slots.by_key.values()), key=lambda s: s["path"].lower()):
        if s["kind"] != "map":
            continue
        real = find_ci(PACK_ROOT, s["path"])
        if not real or not os.path.isfile(real):
            continue
        d = os.path.dirname(s["path"]) + "/"
        for i, nm in enumerate(parse_bd1_textures(real)):
            if not nm:
                continue
            tp = norm(d + nm)
            slots.add(tp, kind="texture", **{"class": "map_tile"}, hard=False,
                      source_refs=["assets/pack/%s#tex%d" % (s["path"], i), maptex_use],
                      priority=s["priority"], summary="Map tile %d of %s." % (i, s["path"]),
                      tileable=True)

    # ---- 6. LoadTexture BlackTransparent audit -------------------------------
    black = audit_black_transparent(src, log)

    # ---- 7. finalise fields --------------------------------------------------
    old_status = {}
    if os.path.exists(OUT_PATH):
        try:
            with open(OUT_PATH, encoding="utf-8") as f:
                for s in json.load(f).get("slots", []):
                    old_status[s["path"].lower()] = s.get("status", "missing")
        except (ValueError, KeyError):
            pass

    out = []
    for key in sorted(slots.by_key):
        s = slots.by_key[key]
        r = ref.get(key)
        cls = s["class"]
        size, fmt = None, None
        if r:
            if r.get("width"):
                size = [r["width"], r["height"]]
            fmt = r.get("format")
        if s["kind"] == "texture" and (size is None or fmt is None):
            dsz, dfmt = DEFAULT_SHAPE.get(cls, (None, None))
            size = size or dsz
            fmt = fmt or dfmt
        if fmt is None:
            fmt = {"model": "x_text", "sound": "wav_pcm16_mono_22k", "map": "bd1",
                   "points": "pd1", "mission": "text_cp932"}.get(s["kind"])
        if s["kind"] == "dir":
            alpha = None
        elif key in black:
            alpha = "black_key"
        elif fmt and fmt.startswith("dds_"):
            alpha = "channel"
        else:
            alpha = "none" if s["kind"] == "texture" else None
        slot = OrderedDict()
        slot["path"] = s["path"]
        slot["kind"] = s["kind"]
        slot["class"] = cls
        slot["hard"] = bool(s.get("hard"))
        slot["source_refs"] = sorted(s["source_refs"], key=_refkey)
        slot["ref_size"] = size
        slot["ref_format"] = fmt
        slot["alpha"] = alpha
        slot["tileable"] = bool(s.get("tileable", False))
        slot["uv_layout"] = s.get("uv_layout")
        slot["summary"] = s.get("summary") or ""
        slot["priority"] = s.get("priority", "P1")
        slot["status"] = old_status.get(key, "missing")
        slot["note"] = s.get("note")
        out.append(slot)

    stats = {
        "hard_literals_distinct": len(lit_refs),
        "hard_literals_by_category": dict(sorted(lit_counts.items())),
        "slots_by_kind": dict(sorted(Counter(s["kind"] for s in out).items())),
        "slots_by_class": dict(sorted(Counter(s["class"] for s in out).items())),
        "slots_by_priority": dict(sorted(Counter(s["priority"] for s in out).items())),
    }
    doc = OrderedDict()
    doc["version"] = 1
    doc["generated_by"] = "tools/manifest/build.py"
    doc["upstream"] = "OpenXOPS 1.080 (src/)"
    doc["reference"] = "tools/manifest/xops096_reference.json"
    doc["stats"] = stats
    doc["dynamic_patterns"] = patterns
    doc["slots"] = out
    return doc, log


def _refkey(r):
    m = re.match(r"^(.*):(\d+)$", r)
    return (m.group(1), int(m.group(2))) if m else (r, 0)


def map_name(d, missions, hv):
    """Map name from the standard mission table (e.g. 'URBAN' from 'URBAN_KT')."""
    names = []
    for i in sorted(missions):
        if norm(hv(missions[i], "directory")).lower() == d.lower():
            nm = str(hv(missions[i], "name")).strip()
            names.append(nm.rsplit("_", 1)[0] if "_" in nm else nm)
    if not names:
        return "no standard mission"
    return Counter(names).most_common(1)[0][0]


def human_model_summary(stem, humans, up_users, weapons, hv, wname):
    m = re.match(r"up(\d)$", stem)
    if m:
        n = int(m.group(1))
        users = sorted({gloss(humans[i]["_comment"]) for i in up_users.get(n, [])})
        return "Upper body mesh, body type %d. Presets: %s." % (n, ", ".join(users) or "none")
    m = re.match(r"arm0(\d)$", stem)
    if m:
        n = int(m.group(1))
        wp = {0: 2, 1: 0, 2: 1}[n]   # ArmModelID <- WeaponP (object.cpp HumanMotionControl)
        ws = sorted({wname(i) for i in weapons if hv(weapons[i], "WeaponP") == wp and hv(weapons[i], "model")})
        pose = {0: "arms down (no weapon or carrying a case)",
                1: "two-handed weapon grip", 2: "one-handed weapon grip"}[n]
        return "Arms mesh: %s. Used with: %s." % (pose, ", ".join(ws) or "no weapon")
    if stem == "leg":
        return "Legs mesh, standing still."
    m = re.match(r"(walk|run)0(\d)$", stem)
    if m:
        total = 4 if m.group(1) == "walk" else 6
        return "Legs mesh, %s cycle keyframe %d of %d (morphed in-between frames)." % (
            "walking" if m.group(1) == "walk" else "running", int(m.group(2)), total)
    return "Human model part."


def sound_summary(stem, code, weapons, smallobj, hv, wname):
    m = re.search(r"soundid\[(\d)\]", code)
    if m and stem.startswith(("bang", "ka")):
        n = int(m.group(1))
        sid = 13 if n == 5 else n            # resource.cpp maps Weapon.soundid 13 -> soundid[5]
        ws = sorted({wname(i) for i in weapons if hv(weapons[i], "soundid") == sid and hv(weapons[i], "model")})
        kind = "suppressed shot" if n == 5 else "gunshot %d" % (n + 1)
        return "Weapon sound, %s. Used by: %s." % (kind, ", ".join(ws) or "none")
    if m:
        n = int(m.group(1))
        users = sorted({gloss(smallobj[i]["_comment"]) for i in smallobj if hv(smallobj[i], "sound") == n})
        return "Small object hit sound %d. Used by: %s." % (n, "; ".join(users))
    what = {"bullet_hitsoundA": "bullet impact on walls and objects (A)",
            "bullet_hitsoundB": "bullet impact on walls and objects (B)",
            "bullet_humanhitsound": "bullet hitting a person",
            "bullet_passingsound": "bullet passing close by",
            "grenade_bang": "grenade explosion", "grenade_cco": "grenade bouncing"}
    for var, txt in what.items():
        if var in code:
            return "Sound: %s (%s)." % (txt, var)
    return "Sound effect."


def audit_black_transparent(src, log):
    """Return lower-case paths loaded with BlackTransparent=true.

    Every LoadTexture(a, b, c) call is listed; c is the BlackTransparent flag.
    Literal first arguments map straight to a path; variable ones map to the
    table that feeds them (weapon/small object/bullet/human/sky/map/briefing).
    """
    hits = set()
    rx = re.compile(r"LoadTexture\(([^,]+),\s*(\w+),\s*(\w+)\)")
    for fname in src.files:
        for no, code, _c in src.code_lines(fname):
            m = rx.search(code)
            if not m or "D3DGraphics::LoadTexture" in code:
                continue
            arg, black = m.group(1).strip(), m.group(3)
            log.append("src/%s:%d LoadTexture(%s, ..., BlackTransparent=%s)" % (fname, no, arg, black))
            if black == "true":
                lm = re.match(r'^"(.*)"$', arg)
                if lm:
                    hits.add(norm(lm.group(1).replace("\\\\", "\\")).lower())
                else:
                    log.append("  UNRESOLVED black-key variable %s at src/%s:%d" % (arg, fname, no))
    return hits


def render(doc):
    return json.dumps(doc, indent=1, ensure_ascii=False) + "\n"


def main():
    ap = argparse.ArgumentParser(description="Generate assets/manifest.json from upstream source.")
    ap.add_argument("--check", action="store_true", help="exit 1 if manifest.json is out of date")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--audit", action="store_true", help="print the LoadTexture BlackTransparent audit")
    a = ap.parse_args()
    doc, log = build()
    text = render(doc)
    if a.audit:
        print("\n".join(log))
    if a.check:
        cur = open(OUT_PATH, encoding="utf-8").read() if os.path.exists(OUT_PATH) else ""
        if cur != text:
            print("assets/manifest.json is out of date; run tools/manifest/build.py")
            return 1
        print("assets/manifest.json is up to date")
        return 0
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    if not a.quiet:
        st = doc["stats"]
        print("wrote assets/manifest.json: %d slots" % len(doc["slots"]))
        print("hard literals: %d distinct %s" % (st["hard_literals_distinct"], st["hard_literals_by_category"]))
        for k in ("slots_by_kind", "slots_by_class", "slots_by_priority"):
            print("%s: %s" % (k, st[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())

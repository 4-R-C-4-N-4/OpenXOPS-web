#!/usr/bin/env python3
"""Check a DirectX .x text model against the OpenXOPS OpenGL loader.

Emulates D3DGraphics::LoadModel (src/d3dgraphics-opengl.cpp:468-721) line by
line, so a file that passes here is parsed the same way by the engine. It also
reports the rules from docs/ASSET-CONTRACT.md section 4 that the parser does not
enforce itself (face size, UV count, winding).

Usage:
    python3 tools/contract/check_x.py FILE.x [--closed] [--morph OTHER.x]

--closed  also check that every face is wound so the engine shows it from
          outside (clockwise seen from outside in file/D3D coordinates).
          Only meaningful for closed, roughly convex parts.
--morph   check that OTHER.x can be morphed with FILE.x (same topology).
Exit status 0 = OK, 1 = errors.
"""
import argparse
import sys

DELIMS = " ;,"


def tok(line):
    # strtok(buf, " ;,"): tabs/newlines are NOT delimiters, but atof/atoi skip
    # leading whitespace, so strip each token.
    out = []
    cur = ""
    for ch in line:
        if ch in DELIMS:
            if cur:
                out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return [t.strip() for t in out if t.strip()]


def atoi(s):
    s = s.lstrip()
    n = ""
    for i, ch in enumerate(s):
        if ch.isdigit() or (i == 0 and ch in "+-"):
            n += ch
        else:
            break
    try:
        return int(n)
    except ValueError:
        return 0


def atof(s):
    s = s.strip()
    best = 0.0
    for i in range(len(s), 0, -1):
        try:
            best = float(s[:i])
            break
        except ValueError:
            continue
    return best


def parse(path):
    """Return (meshes, errors, warnings). Each mesh: dict(verts, faces, uvs)."""
    errors, warns = [], []
    with open(path, "r", encoding="latin-1", newline=None) as fh:
        lines = fh.read().split("\n")
    lines = [ln + "\n" for ln in lines]
    if lines and lines[-1] == "\n":
        lines.pop()
    it = iter(enumerate(lines, 1))

    def nxt():
        try:
            return next(it)
        except StopIteration:
            return None, None

    no, first = nxt()
    if first is None or not first.startswith("xof 0302txt"):
        errors.append("line 1 must start with 'xof 0302txt' (opengl.cpp:506-511)")
        return [], errors, warns
    for _no, ln in enumerate(lines, 1):
        if len(ln) > 255:
            errors.append(f"line {_no}: longer than 255 chars; fgets(buf,256) would split it")
    meshes = []
    while True:
        no, ln = nxt()
        if ln is None:
            break
        if ln.lstrip(" \t").startswith("Mesh ") and not ln.startswith("Mesh"):
            if not ln.lstrip(" \t").startswith(("MeshTextureCoords", "MeshNormals", "MeshMaterialList", "MeshVertexColors")):
                warns.append(f"line {no}: indented 'Mesh' is IGNORED by the engine (only column-0 'Mesh' is parsed)")
        if not ln[:4] == "Mesh":
            continue
        if ln.startswith(("MeshTextureCoords", "MeshMaterialList", "MeshNormals", "MeshVertexColors")):
            errors.append(f"line {no}: '{ln.strip()}' at column 0 is mis-read as a new Mesh; indent it by one space")
        no_c, ln_c = nxt()
        nv = atoi(ln_c or "")
        verts = []
        for _ in range(nv):
            vn, vl = nxt()
            if vl is None:
                errors.append("EOF inside vertex list")
                return meshes, errors, warns
            t = tok(vl)
            if len(t) < 3:
                errors.append(f"line {vn}: vertex needs x;y;z on ONE line")
                t += ["0"] * 3
            verts.append((atof(t[0]), atof(t[1]), atof(t[2])))
        sep_no, sep = nxt()  # one line skipped unconditionally (opengl.cpp:540)
        if sep is not None and sep.strip() and atoi(sep) and len(tok(sep)) == 1:
            errors.append(f"line {sep_no}: this line is SKIPPED by the engine; it looks like the face count. "
                         "Put exactly one separator line (e.g. blank) between the last vertex and the face count")
        fc_no, fcl = nxt()
        nf = atoi(fcl or "")
        faces = []
        for _ in range(nf):
            fn, fl = nxt()
            if fl is None:
                errors.append("EOF inside face list")
                return meshes, errors, warns
            t = [atoi(x) for x in tok(fl)]
            if not t:
                errors.append(f"line {fn}: empty face line")
                continue
            n = t[0]
            if n not in (3, 4):
                errors.append(f"line {fn}: face with {n} vertices; only 3 or 4 are allowed (opengl.cpp:547,599-685)")
            idx = t[1:1 + n]
            if len(idx) != n:
                errors.append(f"line {fn}: face must be written on one line")
            for i in idx:
                if not 0 <= i < nv:
                    errors.append(f"line {fn}: index {i} out of range 0..{nv - 1}")
            faces.append(idx)
        uvs = None
        while True:
            un, ul = nxt()
            if ul is None:
                break
            if ul[:4] == "Mesh":
                warns.append(f"line {un}: column-0 '{ul.strip()}' swallowed while searching for ' MeshTextureCoords'")
            if ul[:18] == " MeshTextureCoords":
                cn, cl = nxt()
                if atoi(cl or "") != nv:
                    errors.append(f"line {cn}: MeshTextureCoords count {atoi(cl or '')} != vertex count {nv}; "
                                  "engine leaves UVs UNINITIALISED (opengl.cpp:571)")
                    break
                uvs = []
                for _ in range(nv):
                    tn, tl = nxt()
                    t = tok(tl or "")
                    if len(t) < 2:
                        errors.append(f"line {tn}: UV needs u;v on ONE line")
                        t += ["0", "0"]
                    uvs.append((atof(t[0]), atof(t[1])))
                break
            if ul.lstrip().startswith("MeshTextureCoords"):
                errors.append(f"line {un}: MeshTextureCoords must be indented by exactly ONE space (opengl.cpp:567-568)")
        if uvs is None:
            errors.append(f"mesh at line {no}: no usable ' MeshTextureCoords' block; UVs undefined")
        meshes.append({"line": no, "verts": verts, "faces": faces, "uvs": uvs})
    if not meshes:
        errors.append("no column-0 'Mesh' found; the engine loads an empty model")
    return meshes, errors, warns


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def check_winding(mesh):
    """Visible side: (v1-v0)x(v2-v0) points toward the viewer (= outward)."""
    v = mesh["verts"]
    if not v:
        return []
    c = tuple(sum(p[i] for p in v) / len(v) for i in range(3))
    bad = []
    for fi, f in enumerate(mesh["faces"]):
        if len(f) < 3:
            continue
        p0, p1, p2 = v[f[0]], v[f[1]], v[f[2]]
        n = cross(sub(p1, p0), sub(p2, p0))
        fc = tuple(sum(v[i][k] for i in f) / len(f) for k in range(3))
        if dot(n, sub(fc, c)) <= 0:
            bad.append(fi)
    return bad


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--closed", action="store_true")
    ap.add_argument("--morph")
    a = ap.parse_args(argv)
    meshes, errors, warns = parse(a.file)
    nv = sum(len(m["verts"]) for m in meshes)
    nf = sum(len(m["faces"]) for m in meshes)
    print(f"{a.file}: {len(meshes)} mesh(es), {nv} vertices, {nf} faces")
    if a.closed:
        for m in meshes:
            bad = check_winding(m)
            if bad:
                errors.append(f"mesh at line {m['line']}: faces {bad} are wound inward (would be culled from outside)")
    if a.morph:
        m2, e2, _ = parse(a.morph)
        errors += [f"{a.morph}: {e}" for e in e2]
        f1 = [f for m in meshes for f in m["faces"]]
        f2 = [f for m in m2 for f in m["faces"]]
        v2 = sum(len(m["verts"]) for m in m2)
        if v2 != nv:
            errors.append(f"morph: vertex count {nv} != {v2} (D3D backend rejects, directx.cpp:642-646)")
        if len(f1) != len(f2):
            errors.append(f"morph: face count {len(f1)} != {len(f2)} (GL backend rejects, opengl.cpp:762)")
        elif f1 != f2:
            errors.append("morph: face lists differ (indices / per-face vertex count must be identical)")
    for w in warns:
        print("WARN ", w)
    for e in errors:
        print("ERROR", e)
    print("OK" if not errors else f"{len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

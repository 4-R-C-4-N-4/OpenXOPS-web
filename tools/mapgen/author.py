"""JSON authoring format for maps: compiles to .bd1 + .pd1 (+ optional
.msg) pairs, and decompiles back to the same JSON for round-tripping
("dump").

This module only *authors* convenient structures; the on-disk byte
format is entirely owned by bd1.py / pd1.py, which encode exactly what
the engine loaders expect (see their docstrings and docs/MAPGEN.md for
source citations).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List

from . import bd1, pd1

SCHEMA_VERSION = 1


class AuthorError(ValueError):
    pass


# ---------------------------------------------------------------------------
# Blocks
# ---------------------------------------------------------------------------

def _build_block(entry: Dict[str, Any], textures: List[str]) -> bd1.Block:
    if "vertices" in entry:
        verts = entry["vertices"]
        if len(verts) != 8:
            raise AuthorError("raw block 'vertices' must have exactly 8 [x,y,z] entries")
        xs = tuple(v[0] for v in verts)
        ys = tuple(v[1] for v in verts)
        zs = tuple(v[2] for v in verts)
        faces_in = entry.get("faces")
        if not faces_in or len(faces_in) != 6:
            raise AuthorError("raw block needs 'faces': 6 entries, each {texture,u,v}")
        faces = []
        for f in faces_in:
            tid = _texture_index(f.get("texture"), textures)
            u = tuple(f.get("u", (0.0, 0.0, 1.0, 1.0)))
            v = tuple(f.get("v", (0.0, 0.0, 1.0, 1.0)))
            faces.append(bd1.FaceMaterial(texture_id=tid, u=u, v=v))
        return bd1.Block(x=xs, y=ys, z=zs, faces=faces)

    if "min" in entry and "max" in entry:
        vmin = entry["min"]
        vmax = entry["max"]
        faces_spec = entry.get("faces", {})
        face_tex = []
        for name in bd1.FACE_NAMES:
            spec = faces_spec.get(name, entry.get("texture"))
            face_tex.append(_texture_index(spec, textures))
        uv_scale = entry.get("uv_scale", [1.0, 1.0])
        return bd1.box_block(vmin, vmax, face_tex, uv_scale=uv_scale)

    raise AuthorError("block entry needs either 'min'/'max' (box) or 'vertices' (raw)")


def _texture_index(spec: Any, textures: List[str]) -> int:
    if spec is None:
        return -1
    if isinstance(spec, bool):
        raise AuthorError("texture reference must be an int index or a string name")
    if isinstance(spec, int):
        return spec
    if isinstance(spec, str):
        if spec not in textures:
            raise AuthorError(f"unknown texture name {spec!r}; not in 'textures' list")
        return textures.index(spec)
    raise AuthorError(f"bad texture reference: {spec!r}")


def _block_to_json(block: bd1.Block, textures: List[str]) -> Dict[str, Any]:
    faces = []
    for name, face in zip(bd1.FACE_NAMES, block.faces):
        tex = textures[face.texture_id] if 0 <= face.texture_id < len(textures) else face.texture_id
        faces.append({"face": name, "texture": tex, "u": list(face.u), "v": list(face.v)})
    return {
        "vertices": [[block.x[i], block.y[i], block.z[i]] for i in range(8)],
        "faces": faces,
    }


# ---------------------------------------------------------------------------
# Points
# ---------------------------------------------------------------------------

# Friendly point kinds -> (p1, field order). See pd1.py docstring for
# the source citations behind each mapping.
def _build_point(entry: Dict[str, Any]) -> pd1.Point:
    kind = entry.get("type")
    pos = entry.get("pos", [0.0, 0.0, 0.0])
    x, y, z = pos
    r = entry.get("rotation", 0.0)

    if kind == "player_spawn":
        # HUMAN2 with p4=0 is the controlled player (objectmanager.cpp:269,1486).
        return pd1.Point(x, y, z, r, p1=pd1.P1_HUMAN2,
                          p2=entry.get("human_type", 0),
                          p3=entry.get("ai_path", 0),
                          p4=0)
    if kind == "human":
        # Enemy/NPC: HUMAN2, p4 != 0 so it is not auto-selected as player.
        team = entry.get("team", 1)
        if team == 0:
            raise AuthorError("human 'team' must be non-zero (0 is reserved for player_spawn)")
        return pd1.Point(x, y, z, r, p1=pd1.P1_HUMAN2,
                          p2=entry.get("human_type", 0),
                          p3=entry.get("ai_path", 0),
                          p4=team)
    if kind == "weapon_pickup":
        # WEAPON: p2 weapon id, p3 ammo count (objectmanager.cpp:384-385).
        return pd1.Point(x, y, z, r, p1=pd1.P1_WEAPON,
                          p2=entry.get("weapon_id", 0),
                          p3=entry.get("ammo", 0),
                          p4=entry.get("p4", 0))
    if kind == "waypoint":
        # AIPATH: p2 mode (event.cpp:179-181).
        return pd1.Point(x, y, z, r, p1=pd1.P1_AIPATH,
                          p2=entry.get("mode", 0),
                          p3=entry.get("p3", 0),
                          p4=entry.get("p4", 0))
    if kind == "small_object":
        # SMALLOBJ: p2 object type, p3 nonzero = extra map collision
        # snap (objectmanager.cpp:477-493).
        return pd1.Point(x, y, z, r, p1=pd1.P1_SMALLOBJ,
                          p2=entry.get("object_type", 0),
                          p3=1 if entry.get("map_collision", True) else 0,
                          p4=entry.get("p4", 0))
    if kind == "event":
        # 10..19 are the engine's recognized "event" p1 range
        # (gamemain.cpp:4022); p2/p3/p4 semantics per-event are not
        # resolved from source read for this task, so they pass through
        # as raw ints.
        event_id = entry.get("event_id")
        if event_id is None or not (pd1.P1_EVENT_MIN <= event_id <= pd1.P1_EVENT_MAX):
            raise AuthorError(f"event_id must be in [{pd1.P1_EVENT_MIN}, {pd1.P1_EVENT_MAX}]")
        return pd1.Point(x, y, z, r, p1=event_id,
                          p2=entry.get("p2", 0), p3=entry.get("p3", 0), p4=entry.get("p4", 0))
    if kind == "raw":
        return pd1.Point(x, y, z, r,
                          p1=entry["p1"], p2=entry.get("p2", 0),
                          p3=entry.get("p3", 0), p4=entry.get("p4", 0))

    raise AuthorError(f"unknown point type {kind!r}")


_HUMAN2_KIND_BY_P4 = None  # placeholder for readability; see _point_to_json


def _point_to_json(p: pd1.Point) -> Dict[str, Any]:
    pos = [p.x, p.y, p.z]
    if p.p1 == pd1.P1_HUMAN2:
        if p.p4 == 0:
            return {"type": "player_spawn", "pos": pos, "rotation": p.r,
                    "human_type": p.p2, "ai_path": p.p3}
        return {"type": "human", "pos": pos, "rotation": p.r,
                "human_type": p.p2, "ai_path": p.p3, "team": p.p4}
    if p.p1 == pd1.P1_WEAPON:
        return {"type": "weapon_pickup", "pos": pos, "rotation": p.r,
                "weapon_id": p.p2, "ammo": p.p3, "p4": p.p4}
    if p.p1 == pd1.P1_AIPATH:
        return {"type": "waypoint", "pos": pos, "rotation": p.r,
                "mode": p.p2, "p3": p.p3, "p4": p.p4}
    if p.p1 == pd1.P1_SMALLOBJ:
        return {"type": "small_object", "pos": pos, "rotation": p.r,
                "object_type": p.p2, "map_collision": bool(p.p3), "p4": p.p4}
    if pd1.P1_EVENT_MIN <= p.p1 <= pd1.P1_EVENT_MAX:
        return {"type": "event", "pos": pos, "rotation": p.r,
                "event_id": p.p1, "p2": p.p2, "p3": p.p3, "p4": p.p4}
    return {"type": "raw", "pos": pos, "rotation": p.r,
            "p1": p.p1, "p2": p.p2, "p3": p.p3, "p4": p.p4}


# ---------------------------------------------------------------------------
# Top-level build / dump
# ---------------------------------------------------------------------------

@dataclass
class BuildResult:
    bd1_bytes: bytes
    pd1_bytes: bytes


def build(doc: Dict[str, Any]) -> BuildResult:
    textures = doc.get("textures", [])
    if not isinstance(textures, list):
        raise AuthorError("'textures' must be a list of strings")

    blocks = [_build_block(b, textures) for b in doc.get("blocks", [])]
    bd1_file = bd1.BD1File(textures=textures, blocks=blocks)

    points = [_build_point(p) for p in doc.get("points", [])]
    pd1_file = pd1.PD1File(points=points)

    return BuildResult(bd1_bytes=bd1.dumps(bd1_file), pd1_bytes=pd1.dumps(pd1_file))


def dump_bd1(data: bytes) -> Dict[str, Any]:
    f = bd1.loads(data)
    return {
        "schema": SCHEMA_VERSION,
        "textures": f.textures,
        "blocks": [_block_to_json(b, f.textures) for b in f.blocks],
    }


def dump_pd1(data: bytes) -> Dict[str, Any]:
    f = pd1.loads(data)
    return {"schema": SCHEMA_VERSION, "points": [_point_to_json(p) for p in f.points]}

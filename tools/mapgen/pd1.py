"""Reader/writer for OpenXOPS ``.pd1`` point-data files.

Byte layout derived from ``PointDataInterface::LoadFiledata``
(src/datafile.cpp:416-487) and the ``pointdata`` struct
(src/datafile.h:85-96).

File layout (little-endian):

  offset 0   2 bytes          point count, uint16 LE (datafile.cpp:437),
                               clamped to MAX_POINTS=256 (datafile.h:39).
  offset 2+  20 bytes each    one record per point:
       float32 x, y, z, r     (datafile.cpp:446-450; r = rotation)
       int8    p1, p2, p3, p4 (datafile.cpp:453-457; stored widened to
                               ``signed short int`` in memory, but each
                               is read from exactly ONE signed byte on
                               disk, so file-level range is -128..127)

Point ``p1`` type codes (datafile.h:99-106)::

    1  HUMAN         5  SMALLOBJ
    2  WEAPON        6  HUMAN2
    3  AIPATH        7  RAND_WEAPON
    4  HUMANINFO     8  RAND_AIPATH

The engine also recognizes 10..19 as mission/event point types
(gamemain.cpp:4022, ``(10 <= pdata.p1)&&(pdata.p1 <= 19)``), tallied as
"EventPoints" in the in-game debug HUD; beyond that range check, the
*meaning* of a given event id's p2/p3/p4 is not pinned down by the
engine source read for this task (the debug overlay just prints the
four raw ints, gamemain.cpp:3677,3697) so this module exposes a raw
p1..p4 escape hatch for events rather than guessing field semantics.

What *is* confirmed from source, cited per field, for the structured
point kinds below:

  * HUMAN2 (objectmanager.cpp:1481-1491, 269): p2 = human type id
    (``Human[]`` index), p3 = AI path id, p4 = team/slot id -- the
    first HUMAN/HUMAN2 point with p4==0 becomes the controlled player
    (objectmanager.cpp:269, 1486).
  * HUMANINFO (objectmanager.cpp:1448-1465,1495): p2 = human type id;
    a HUMAN (non-2) point looks up its HUMANINFO companion by id (p2
    matches the HUMAN point's own p2 as a foreign key,
    datafile.h SearchPointdata pattern) to resolve type/texture.
  * WEAPON / RAND_WEAPON (objectmanager.cpp:384-399): p2 = weapon id
    (or first random choice), p3 = ammo count / second random choice.
  * AIPATH / RAND_AIPATH (event.cpp:179-181): p2 = path "mode", mutable
    at runtime by SetParam; p3/p4 not resolved from the lines read.
  * SMALLOBJ (objectmanager.cpp:477-493): p2 = small-object type id,
    p3 = nonzero enables extra map-collision snap, p4 = passed through
    to SetParamData but not decoded further here.

This module's ``Point`` dataclass stores the raw (p1, p2, p3, p4) plus
x/y/z/r, and ``author.py`` builds friendly point kinds (player_spawn,
human, weapon_pickup, waypoint, small_object, event) on top of it.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import List

MAX_POINTS = 256       # datafile.h:39
RECORD_SIZE = 20       # 4 floats + 4 int8

P1_HUMAN = 1
P1_WEAPON = 2
P1_AIPATH = 3
P1_HUMANINFO = 4
P1_SMALLOBJ = 5
P1_HUMAN2 = 6
P1_RAND_WEAPON = 7
P1_RAND_AIPATH = 8
P1_EVENT_MIN = 10
P1_EVENT_MAX = 19
# POINT_P1TYPE_STAGED_HUMAN: a held/disabled human (not drawn, not
# simulated, not counted alive) with the same parameter layout as
# P1_HUMAN (p2=HUMANINFO id, p3=AI path, p4=wave group id). Added by the
# concurrent engine worker per docs/MISSION-UFO-FIELD.md E5; classic data
# never uses p1=50.
P1_STAGED_HUMAN = 50


class PD1Error(ValueError):
    """Raised for malformed .pd1 data or author input."""


@dataclass
class Point:
    x: float
    y: float
    z: float
    r: float = 0.0
    p1: int = 0
    p2: int = 0
    p3: int = 0
    p4: int = 0

    def __post_init__(self):
        for name in ("p1", "p2", "p3", "p4"):
            v = getattr(self, name)
            if not (-128 <= v <= 127):
                raise PD1Error(f"{name}={v} out of signed-byte range (-128..127)")


@dataclass
class PD1File:
    points: List[Point] = field(default_factory=list)

    def __post_init__(self):
        if len(self.points) > MAX_POINTS:
            raise PD1Error(f"at most {MAX_POINTS} points (MAX_POINTS, datafile.h:39)")


def dumps(pd1: PD1File) -> bytes:
    out = bytearray()
    out += struct.pack("<H", len(pd1.points))
    for p in pd1.points:
        out += struct.pack("<4f", p.x, p.y, p.z, p.r)
        out += struct.pack("<4b", p.p1, p.p2, p.p3, p.p4)
    return bytes(out)


def loads(data: bytes) -> PD1File:
    if len(data) < 2:
        raise PD1Error("file too short for .pd1 header")
    (count,) = struct.unpack_from("<H", data, 0)
    if count > MAX_POINTS:
        count = MAX_POINTS  # datafile.cpp:438
    pos = 2
    points = []
    for _ in range(count):
        if pos + RECORD_SIZE > len(data):
            raise PD1Error("truncated point record")
        x, y, z, r = struct.unpack_from("<4f", data, pos)
        p1, p2, p3, p4 = struct.unpack_from("<4b", data, pos + 16)
        points.append(Point(x=x, y=y, z=z, r=r, p1=p1, p2=p2, p3=p3, p4=p4))
        pos += RECORD_SIZE
    return PD1File(points=points)

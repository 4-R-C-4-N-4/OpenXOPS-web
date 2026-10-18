# Map authoring tools (`.bd1` / `.pd1`)

Phase B deliverable for `docs/OPEN-ASSETS-SOW.md`. These formats are
derived **only** from the BSD-licensed engine loaders in `src/`; no
original game data file was opened while writing this tool or doc.

Code: `tools/mapgen/` (`bd1.py`, `pd1.py`, `author.py`, `__main__.py`).
Tests: `tools/tests/test_mapgen.py`.
Example: `tools/mapgen/examples/starter.json`.

## CLI

```sh
python3 -m tools.mapgen build map.json -o outdir/        # -> outdir/temp.bd1, outdir/mission.pd1
python3 -m tools.mapgen build map.json -o outdir/ --bd1-name map.bd1 --pd1-name map.pd1
python3 -m tools.mapgen dump some.bd1                     # -> JSON on stdout
python3 -m tools.mapgen dump some.pd1                     # -> JSON on stdout
```

## `.bd1` block-data format

Source: `BlockDataInterface::LoadFiledata`, `src/datafile.cpp:109-176`;
structs `blockdata`/`b_material`, `src/datafile.h:60-82`; constants
`MAX_BLOCKS`/`TOTAL_BLOCKTEXTURE`, `src/datafile.h:35-36`.

All numbers little-endian.

| Offset | Size | Field |
|---|---|---|
| 0 | 10 × 31 bytes | texture names. Each is a *raw, unprefixed* 31-byte field (`fread(texture[i], 1, 31, fp)`, datafile.cpp:129) later `strcpy`'d, so each name **must be NUL-terminated within 31 bytes** (≤30 ASCII chars). An all-zero (empty) slot means "no texture" for that index. |
| 310 | 2 | block count, `uint16` LE (datafile.cpp:133-134); the loader clamps any larger value down to `MAX_BLOCKS` = 160 (datafile.cpp:135, datafile.h:35) and simply ignores further bytes. |
| 312 + 316·i | 316 | one block record (below), `i` = 0..count-1 |

Each 316-byte block record is two consecutive `fread`s in the source
(datafile.cpp:140-141):

1. **288 bytes = 72 × `float32` LE**, consumed into a flat
   `bdata_main[80]` array (datafile.cpp:115) and sliced as:
   * `[0:8]` vertex X, `[8:16]` vertex Y, `[16:24]` vertex Z
     (datafile.cpp:148-150)
   * `[24:48]` UV **U** coordinates, 4 per face, **face-major**:
     face *j*'s 4 U values sit at indices `24+j*4 .. 24+j*4+3`
     (datafile.cpp:156,158,160,162 — `bdata_main[j*4+24..27]`)
   * `[48:72]` UV **V** coordinates, same face-major layout, offset by
     24 (datafile.cpp:157,159,161,163 — `bdata_main[j*4+48..51]`)
2. **28 bytes = 28 × `int8`**, consumed into `bdata_mainb[30]`
   (datafile.cpp:116,141). Only one byte per face is read back: face
   *j*'s `textureID` is the **signed** byte at offset `j*4`
   (datafile.cpp:155, `(int)bdata_mainb[j*4]`). The remaining 3 bytes
   of each 4-byte slot, and the trailing 4 bytes of the 28, are never
   read — this tool writes them as zero.

### Vertex numbering and face → vertex/UV mapping

The vertex-index diagram and the face table both come from the
`blockdataface()` comment and body (`src/datafile.cpp:302-381`):

```
    3 .----. 2
     /|    /|
    0 .----. 1 |
  7 . -|  --. 6
    |  /   | /
    4 .----. 5
```

| Face id | Name | Vertex order | UV slot order |
|---|---|---|---|
| 0 | top | 1,0,3,2 | 2,3,0,1 |
| 1 | bottom | 6,7,4,5 | 2,3,0,1 |
| 2 | front | 5,4,0,1 | 2,3,0,1 |
| 3 | right | 6,5,1,2 | 2,3,0,1 |
| 4 | back | 7,6,2,3 | 2,3,0,1 |
| 5 | left | 4,7,3,0 | 2,3,0,1 |

"UV slot order 2,3,0,1" means: the face's first listed vertex is
textured with `u[j][2],v[j][2]`; the second with `u[j][3],v[j][3]`; the
third with `u[j][0],v[j][0]`; the fourth with `u[j][1],v[j][1]`. This
mapping is fixed by the engine for every face — it is **not** an
authoring choice. `bd1.py` stores/reads `u`/`v` in raw file-slot order
(slot 0..3); `author.py`'s box helper applies the permutation above so
JSON authors can think in plain per-vertex UV order.

### Shading and texture rendering notes (context, not file format)

`CalculationBlockdata()` (datafile.cpp:181-268) computes a per-face
normal and a `shadow` scalar from a fixed light direction
(`LIGHT_RX`/`LIGHT_RY`, datafile.h:49-50) — this is derived at load
time, **not stored in the file**, so it has no effect on the byte
format. Map tiles are rendered with `GL_REPEAT` (no wrap call is ever
issued, so the GL default applies) — tile textures should be seamless.

### Limits

* `MAX_BLOCKS` = 160 blocks (datafile.h:35)
* `TOTAL_BLOCKTEXTURE` = 10 texture slots (datafile.h:36)
* Texture names ≤ 30 ASCII chars (31-byte field, NUL-terminated)

## `.pd1` point-data format

Source: `PointDataInterface::LoadFiledata`, `src/datafile.cpp:416-487`;
struct `pointdata`, `src/datafile.h:85-96`; `MAX_POINTS`, datafile.h:39;
P1 type constants, datafile.h:99-106.

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | point count, `uint16` LE (datafile.cpp:437), clamped to `MAX_POINTS` = 256 (datafile.cpp:438, datafile.h:39) |
| 2 + 20·i | 20 | one point record (below) |

Each 20-byte point record (datafile.cpp:446-457):

* `float32` LE `x, y, z, r` (16 bytes) — `r` is rotation.
* 4 × `int8` `p1, p2, p3, p4` (4 bytes) — widened to `signed short` in
  memory (`pointdata.p1..p4` are `signed short int`, datafile.h:92-95)
  but **each occupies exactly one signed byte on disk**, so the file
  range is -128..127 even though the in-memory type is wider.

### `p1` type codes (`src/datafile.h:99-106`)

| Value | Name |
|---|---|
| 1 | `POINT_P1TYPE_HUMAN` |
| 2 | `POINT_P1TYPE_WEAPON` |
| 3 | `POINT_P1TYPE_AIPATH` |
| 4 | `POINT_P1TYPE_HUMANINFO` |
| 5 | `POINT_P1TYPE_SMALLOBJ` |
| 6 | `POINT_P1TYPE_HUMAN2` |
| 7 | `POINT_P1TYPE_RAND_WEAPON` |
| 8 | `POINT_P1TYPE_RAND_AIPATH` |

The engine also treats `10 <= p1 <= 19` as mission "event" points
(`gamemain.cpp:4022`, tallied as `EventPoints` in the debug HUD). The
per-event meaning of `p2..p4` within that range is **not determined**
by the lines of source read for this task (the debug overlay only
prints the four raw ints back, `gamemain.cpp:3677,3697`); this tool
exposes a raw `p1..p4` escape hatch for events rather than guessing.

### Confirmed `p2`/`p3`/`p4` semantics used by this tool's friendly types

* **HUMAN2** (`objectmanager.cpp:1481-1491,269`): `p2` = human type id,
  `p3` = AI path id, `p4` = team/slot id. The **first** HUMAN/HUMAN2
  point with `p4 == 0` becomes the player-controlled human
  (`objectmanager.cpp:269,1486`).
* **WEAPON / RAND_WEAPON** (`objectmanager.cpp:384-399`): `p2` = weapon
  id (or first random choice), `p3` = ammo count (or second random
  choice).
* **AIPATH / RAND_AIPATH** (`event.cpp:179-181`): `p2` = path "mode"
  (mutable at runtime via `SetParam`); `p3`/`p4` not resolved here.
* **SMALLOBJ** (`objectmanager.cpp:477-493`): `p2` = small-object type
  id, `p3` nonzero enables an extra map-collision snap, `p4` passed
  through but not decoded further by the lines read.
* **HUMANINFO** (`objectmanager.cpp:1448-1465,1495`): `p2` = human type
  id; a plain HUMAN point looks up its companion HUMANINFO point to
  resolve type/texture (foreign-key style, via `SearchPointdata`).

## Author JSON schema

Top-level object:

```json
{
  "schema": 1,
  "textures": ["wall.bmp", "floor.bmp", "crate.bmp"],
  "blocks": [ ... ],
  "points": [ ... ]
}
```

`textures` — up to 10 names (≤30 ASCII chars each); referenced from
blocks either by this list index or by name string.

### Block entries

Axis-aligned box (auto-generates 8 vertices + per-face UV tiling):

```json
{ "min": [x0,y0,z0], "max": [x1,y1,z1],
  "texture": "wall.bmp",
  "faces": {"top": "floor.bmp"},
  "uv_scale": [8, 1] }
```

`texture` sets a default for all 6 faces; `faces` (optional) overrides
individual faces by name (`top`, `bottom`, `front`, `right`, `back`,
`left`). `uv_scale` is `[su, sv]`, a tiling multiplier applied to a
0..1 UV square per face (so `uv_scale: [8,1]` repeats the texture 8×
along that face's "width" axis).

Raw 8-vertex block, for non-box geometry:

```json
{ "vertices": [[x,y,z], ... 8 total],
  "faces": [ {"texture": 0, "u":[0,0,1,1], "v":[0,0,1,1]}, ... 6 total ] }
```

`faces` here is an ordered list of 6 (one per face id 0..5, same order
as the box-face names above) giving raw file-slot `u`/`v` (see the
"UV slot order" note above — these are *not* plain per-vertex order).

### Point entries

| `type` | Required fields | Maps to |
|---|---|---|
| `player_spawn` | `pos` | HUMAN2, `p4=0` |
| `human` | `pos`, `team` (≠0) | HUMAN2, `p4=team` |
| `weapon_pickup` | `pos`, `weapon_id` | WEAPON |
| `waypoint` | `pos` | AIPATH |
| `small_object` | `pos`, `object_type` | SMALLOBJ |
| `event` | `pos`, `event_id` (10..19) | raw event point |
| `raw` | `pos`, `p1` | any p1/p2/p3/p4 combination |

All types accept `rotation` (defaults to 0.0). See `author.py` for the
exact optional-field defaults, and `examples/starter.json` for a full
example map (walled courtyard, 2 crates, player spawn, 2 enemies, one
weapon pickup, one goal event).

## Round-trip guarantee

`bd1.dumps(bd1.loads(data)) == data` and `pd1.dumps(pd1.loads(data)) ==
data` for any well-formed file within the documented limits (byte-exact
round trip, verified by `tools/tests/test_mapgen.py`). `author.build()`
output, dumped back with `author.dump_bd1`/`dump_pd1` and rebuilt,
reproduces the same bytes (JSON field order/comments are not
preserved, but the compiled binary is).

## Engine-loader proof

`tests/mapgen/load_harness.cpp` links directly against
`src/datafile.cpp` and calls the real `BlockDataInterface`/
`PointDataInterface::LoadFiledata` on the files this tool emits,
printing the block/point counts the engine itself parsed. Run via
`tools/tests/test_mapgen.py` (skipped automatically if `g++` is not
available).

# Starter mission: STARTER compound

Phase C deliverable for `docs/OPEN-ASSETS-SOW.md`. Original, clean-room
mission content authored entirely with `tools/mapgen` (Phase B tool),
so no original game data file was opened while writing this content.

## Files

| File | Source JSON | Built with |
|---|---|---|
| `assets/pack/data/starter/temp.bd1` | `tools/mapgen/examples/starter_compound.json` | `python3 -m tools.mapgen build ... --pd1-name start.pd1` |
| `assets/pack/data/starter/start.pd1` | same | same |
| `assets/pack/data/starter/start.txt` | hand-authored | plain `.txt` briefing (see below) |
| `assets/pack/data/map10/temp.bd1` | `tools/mapgen/examples/map10_demo.json` | `python3 -m tools.mapgen build ... --pd1-name op.pd1` |
| `assets/pack/data/map10/op.pd1` | same | same |
| `assets/pack/data/missions.txt` | hand-authored | pipe-delimited mission list line |

Rebuild commands:

```sh
python3 -m tools.mapgen build tools/mapgen/examples/starter_compound.json \
    -o assets/pack/data/starter/ --pd1-name start.pd1
python3 -m tools.mapgen build tools/mapgen/examples/map10_demo.json \
    -o assets/pack/data/map10/ --pd1-name op.pd1
```

`tools/tests/test_starter_content.py` asserts the committed binaries
are byte-identical to a fresh build from the committed JSON, so the
two can never silently drift.

## Starter compound design

A small walled compound (40 blocks, well within `MAX_BLOCKS` = 160,
`src/datafile.h:35`):

* a ground slab and a 40-unit-tall perimeter wall with a gate gap on
  the south side (player entry point);
* a single building (two rooms split by an interior partition wall
  with its own doorway), with its own floor and roof;
* a 4-legged watchtower with a railed platform near the gate;
* 15 crate blocks and one full-height spawn barricade for cover.

Points (15 authored plus 2 synthesized HUMANINFO records, well within `MAX_POINTS` = 256, `src/datafile.h:39`):

* 1 `player_spawn` (HUMAN2, `p4=0` — the first such point becomes the
  controlled player, `objectmanager.cpp:269,1486`) just inside the
  south gate, holding an M92F via Human[2]'s loadout. When only one weapon
  slot is populated, the engine selects that usable slot; nearby pickups
  remain independent WEAPON points;
* 2 `weapon_pickup` points: an **M92F** pistol (`weapon_id=3`, per
  `src/parameter.cpp:606` — `Weapon[3].name = "M92F"`) near the gate,
  and an **MP5** SMG (`weapon_id=1`, `src/parameter.cpp:536`) inside
  the building's east room;
* 6 `waypoint` points (AIPATH, `event.cpp:179-181`) forming a patrol
  loop around the courtyard and building;
* 5 `human` points (enemy guards, HUMAN2 with `team=2`, `human_type=2`),
  each referencing one of the 6 waypoints via `ai_path`
  (`objectmanager.cpp:1481-1491`);
* 1 `event` point (`event_id=10`, the engine's recognized
  "event" `p1` range `10..19`, `gamemain.cpp:4022`) in the building's
  inner room, used as the mission's reach/clear objective marker. The
  per-event `p2..p4` semantics in that range are not resolved from the
  lines of source read for Phase B/C (see `docs/MAPGEN.md`'s note on
  this); engine-side event handling for p1=10 is being built in
  parallel per the task brief, so this point is a grounded placeholder
  consistent with the documented range, not a guessed payload.

### Engine-scale and survivable-spawn correction

Engine humans are 20 units tall (`src/object.h:HUMAN_HEIGHT`), with a 2.8-unit
collision radius. The initial authored design used 5-unit walls and 2-unit
crates: it was undersized, not evidence that its floor collision was broken.
Parent QA readback showed HP 110 -> 19 -> -64 in about a second while feet
stayed at y=0. The M92F was present initially; NONE appeared after death/drop.

The starter JSON now uses eight engine units per initial design unit. The
spawn barricade is wide and taller than a standing human. Production browser
verification (no ZIP and no QA exports): survived 30 seconds; W moved camera
z159.92 ->154.72 with ground-level feet; M92F fired and the ammo count fell.
QA-build readback independently retained HP110 after 30 seconds. The real
engine collision harness is run from the test suite, not only manually.
This is a starter prototype, not a completed campaign/balance pass.

### Bug fix: mission ended instantly with no player control

The mission used to end (result/statistics screen) within ~2 seconds
of starting, with movement keys having no effect. Root cause, from
engine source:

* `ObjectManager::CheckGameOverorComplete()` (`objectmanager.cpp:2575`)
  declares "mission complete" the moment the live total HP of every
  human whose team differs from the player's reaches 0 — i.e. zero
  *counted* enemies, not necessarily zero living humans.
* A human's in-game team comes from its `HUMANINFO` companion point's
  `p3` field, resolved by `ObjectManager::AddHumanIndex(pointdata,
  pointdata, bool)` (`objectmanager.cpp:171-236`, team set at line 215
  via `SetParamData(infodata.p2, ..., infodata.p3, true)`), found via
  `PointData->SearchPointdata(&humaninfodata, 0x01+0x08,
  POINT_P1TYPE_HUMANINFO, 0, 0, data.p2, 0)` (`objectmanager.cpp:1465`)
  — a `p1==HUMANINFO && p4==<the HUMAN2 point's own p2>` lookup.
* The original `starter_compound.json` (and `tools/mapgen/author.py`)
  never emitted any `HUMANINFO` points, so that lookup always failed
  and every human (player and all 5 enemies) fell into the "mystery
  human" fallback (`objectmanager.cpp:1468-1491`), which calls
  `AddHumanIndex(..., TeamID=0, ...)` with team **hardcoded to 0**
  regardless of the authored `team`/p4 value. Player and enemies thus
  shared team 0, `CheckGameOverorComplete()` counted zero hostile HP,
  and the mission "completed" on the very first processed frame —
  before any movement input mattered.

Fix: `tools/mapgen/author.py` now auto-synthesizes one `HUMANINFO`
point per distinct `(human_type, team)` pair used by `player_spawn`/
`human` points (`_synthesize_human_info()`), and rewrites each
HUMAN2 point's `p2` into the matching HUMANINFO foreign key. This is
transparent to the authored JSON (field names/values unchanged;
`author.dump_pd1()` resolves the foreign key back to a plain
`human_type` for round-tripping) and applies to every mapgen-built
mission, not just this one. `assets/pack/data/starter/start.pd1` and
`assets/pack/data/map10/op.pd1` were rebuilt from their JSON sources
to pick up the fix; see "Rebuild commands" above.

Verified in the browser build (`tools/build-playable-web.sh`): the
mission now stays in "Game running" state (no result/statistics
screen) for 30+ seconds after the briefing, versus ~2 seconds before
the fix.

All five generic texture names used (`wall.bmp`, `floor.bmp`,
`crate.bmp`, `roof.bmp`, `ground.bmp`) are listed in
`assets/batches/starter_textures.json` for the next Codex image-gen
batch; no images were generated by this task.

## Briefing text (`start.txt`)

Format per `MIFInterface::LoadDefaultTextFiledata`
(`src/datafile.cpp:789-838`, taken because the `.txt` extension check
at `src/datafile.cpp:705-709` routes plain-text mission files there
instead of the binary `.mif` path):

1. line 1: picture A filename stem, or `!` for none — `!` here;
2. line 2: picture B filename stem, or `!` — `!` here;
3. line 3: sky number (`atoi`'d, used by `Resource.LoadSkyModelTexture`,
   `src/resource.cpp:751-764`) — `0` here;
4. up to 17 further lines of briefing text, each read with
   `fgets(str, 50, fp)` (so each line must be under 49 characters, or
   it silently splits across two of those 17 slots) — 16 lines used
   here.

## `data/missions.txt` line

```
STARTER|STARTER compound|data\starter\|start|0|0
```

Fields: `name|fullname|directory|txtbase|collision|screen`. Engine
support for reading this list is being built in parallel (per the task
brief); this line matches the directory/txtbase/collision/screen
fields `MIFInterface::LoadMissionInfoFiledata`/
`LoadDefaultTextFiledata` already expect once pointed at
`data\starter\start.txt`.

## Menu demo map (`data/map10/`)

`opening::Create()` (`src/gamemain.cpp:217-271`) always loads
`data\map10\temp.bd1` / `data\map10\op.pd1` for the title screen, sets
`ObjMgr.SetPlayerID(MAX_HUMAN-1)` (an out-of-range human slot, so no
loaded `HUMAN2` point becomes a real controlled player — any `team`
value works for demo figures), and places a static camera at
`(-5, 58, 29)` with yaw `DegreeToRadian(206)` / pitch
`DegreeToRadian(12)` (`src/gamemain.cpp:266-270`) — an elevated,
downward-looking view toward the world origin.

`tools/mapgen/examples/map10_demo.json` builds a small open yard
(16 blocks) with one building and three crates centered near the
origin so it reads from that camera, plus 2 patrolling `human` points
walking between 3 waypoints so the title screen has visible moving
figures.

## Tests

* `python3 -m unittest discover -s tools/tests` — includes
  `test_starter_content.py`: binary-matches-JSON-source determinism,
  block/point count limits, player-spawn-in-bounds-and-on-floor,
  enemy/waypoint/weapon/event presence, `start.txt` format, the
  `missions.txt` line shape, and (when `g++` is available) the real
  `BlockDataInterface`/`PointDataInterface::LoadFiledata` engine-loader
  proof on the committed `starter` and `map10` binaries.

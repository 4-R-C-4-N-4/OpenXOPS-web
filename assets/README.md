# assets/: Free Data Pack manifest, pack and provenance

This directory holds the CC0 replacement for the X operations data files
(see `docs/FREE-DATA-PACK.md`). Nothing from the original game is stored here.

| Path | What it is | Edited by |
|---|---|---|
| `manifest.json` | Every slot (file path) the engine needs, plus its shape constraints | `tools/manifest/build.py` (only `status` is manual) |
| `pack/` | The Free Pack itself, rooted at `pack/data/` | art pipeline / curators |
| `provenance.jsonl` | One JSON record per file in `pack/` | art pipeline / curators |
| `ASSETS.md` | Human-readable credits table | `tools/validate_pack.py --write-assets-md` |

## Tools

```sh
# Regenerate the manifest from src/ and the reference metadata (deterministic).
python3 tools/manifest/build.py            # writes assets/manifest.json
python3 tools/manifest/build.py --check    # exit 1 if manifest.json is stale
python3 tools/manifest/build.py --audit    # list every LoadTexture call + BlackTransparent flag

# Refresh the factual XOPS 0.96 metadata (path, size, md5, width/height/format).
python3 tools/manifest/fetch_reference.py  # fetches openxops.net -> tools/manifest/xops096_reference.json

# Validate a pack (directory that contains data/).
python3 tools/validate_pack.py assets/pack                 # exit 0 ok, 1 failures
python3 tools/validate_pack.py assets/pack --strict        # warnings become failures
python3 tools/validate_pack.py assets/pack -v              # also list missing P1/P2 slots
python3 tools/validate_pack.py assets/pack --write-assets-md

# Tests (stdlib unittest, synthetic fixtures).
python3 -m unittest discover -s tools/tests -v
```

`build.py` reads upstream source as CP932, scans every non-comment string
literal starting with `data\` or `./data/` (152 distinct), adds the paths the
engine builds at run time (standard missions, menu demos, `sky%d.bmp`), and
fills shapes from `tools/manifest/xops096_reference.json`. It keeps the
`status` of existing slots, so re-running never loses manual edits. If the
pack already contains its own `data/mapN/temp.bd1`, the tile names in that
file's header are added as `map_tile` slots.

`validate_pack.py` checks (FAIL = exit 1):

| Check | Rule | Level |
|---|---|---|
| a | Slot present (case-insensitive). P0 missing = FAIL, P1/P2 missing = WARN. Files that are not slots = WARN. | FAIL/WARN |
| b | Header bytes: textures BM/DDS/FFD8/89PNG (the engine sniffs headers); models start `xof 0302txt`; sounds RIFF/WAVE; `.bd1`/`.pd1` record sizes and the engine's 160-block / 256-point limits | FAIL (format differs from reference = WARN) |
| c | Texture size vs `ref_size` | WARN (`--strict`: FAIL) |
| d | BMP `biCompression == 0`, 4/8/24/32 bpp, bottom-up. DDS uncompressed 16/32 bpp (no FourCC) | FAIL |
| e | WAV `wFormatTag == 1` (PCM). Not mono/22050 Hz/16-bit = WARN | FAIL |
| f | Clean-room guard: MD5 equals any original XOPS 0.96 file | FAIL |
| g | Every pack file has a `provenance.jsonl` record with a matching sha256 | FAIL |

Always write BMPs with `magick ... -compress None BMP3:out.bmp`. ImageMagick's
`BMP3:` default is RLE8 and fails check d.

## Manifest fields

Schema per `docs/FREE-DATA-PACK.md` §4, plus `note`.

| Field | Meaning |
|---|---|
| `path` | Pack-relative path, forward slashes, original case. The engine and validator match it case-insensitively. Dir slots end in `/`. |
| `kind` | `texture`, `model`, `sound`, `map` (`.bd1`), `points` (`.pd1`), `mission` (`.txt` briefing script, `.msg` event messages), `dir` |
| `class` | `hud`, `effect`, `skin`, `weapon_skin`, `article_skin`, `sky`, `map_tile`, `briefing`, `model`, `sfx`, `other` |
| `hard` | `true` if the path (or its `sprintf` pattern) is a string literal in `src/`. `false` if the engine assembles it at run time. |
| `source_refs` | `src/<file>:<line>` of the literal or of the code that builds the path |
| `ref_size` | `[w, h]` from the XOPS 0.96 file list, otherwise from F-4 class defaults. Textures only. |
| `ref_format` | `bmp8`, `bmp24`, `dds_argb8`, `dds_argb4`, `x_text`, `wav_pcm16_mono_22k`, `bd1`, `pd1`, `text_cp932` |
| `alpha` | `none` (BMP), `channel` (DDS with an alpha channel), `black_key` (loaded with `BlackTransparent=true`). `null` for non-textures. |
| `tileable` | `true` only for `map_tile` slots |
| `uv_layout` | The `.x` model whose UVs the texture wraps (skins, weapon/article skins, sky) |
| `summary` | Our own description, derived from source names and comments. Feeds the art prompt. |
| `priority` | `P0`, `P1`, `P2` (see below) |
| `status` | `missing`, `generated`, `curated`, `final`. The only manual edit. |
| `note` | Extra facts: dynamic path patterns, optional files, other body models a skin is worn on |

The top level also carries `stats` (literal and slot counts) and
`dynamic_patterns` (each run-time path pattern with its source lines).

**`black_key` is unused today.** Every `LoadTexture` call in OpenXOPS 1.080
passes `BlackTransparent=false` (`build.py --audit` lists all 28 calls), so no
slot is black-keyed. The audit flags any `true` call that appears later.

## Priorities

**P0: boot to the title screen and play one mission.** Each item follows from
what the engine loads on that path and what makes it stop:

- `data/char.dds`: font and HUD atlas, loaded by `InitD3D` before anything else.
- `data/title.dds`: the menu and briefing logo.
- Opening: `data/map10/temp.bd1` and `data/map10/op.pd1`. A missing file shows
  "block/point data open failed" (gamemain.cpp:224-233). Sky 1 plus `data/sky/sky.x`
  (gamemain.cpp:255).
- Menu: `mainmenu::Create` picks one of six demo maps at random (`GetRand(6)`,
  gamemain.cpp:539). If that demo's `temp.bd1` or `demo.pd1` is missing, the
  menu shows the same error. So all six (map2, map4, map5, map7, map8, map16)
  are P0. Otherwise the menu fails about 5 times in 6.
- One mission: **ALLEY_EXT** (`data/map10/ext.pd1`, `ext.txt`). It reuses
  `map10/temp.bd1`, which the opening already needs, so it costs only two small
  files. The `data/briefing/` dir is P0 because the mission script names its
  pictures there.
- Human rig: `up0-5.x` (every preset uses one of them), `arm00-02.x` (one per
  grip type), `leg.x`, `walk01-04.x`, `run01-06.x`.
- Skins for two presets that cover both common body types:
  `soldier_green.bmp` (special forces green A-C, body up0) and `soldier0.bmp`
  (soldier 1 A, body up4).
- Core weapons: the ones those two presets carry (M92F, MAC10, DESERT EAGLE, M4)
  plus GRENADE, which the bullet table loads as the thrown object (`Bullet[1]`).
  That gives a pistol, an SMG, a rifle and a grenade.
- `bullet.x`/`bullet.bmp`, the five effect DDS files (blood, mflash, smoke,
  yakkyou, scope) and all 14 sounds (they load as one block at init).

**P1:** the rest of the hard slots (other skins, weapons, small objects,
skies 2-5), plus the optional `.msg` for the P0 mission and opening.
**P2:** the 57 other standard missions (block map, point file, briefing
script, optional `.msg`) and their map dirs.

Missing models and textures don't crash the engine (F-2); they just don't
render. P0 is what lets a player actually see and play.

## Provenance record

One JSON object per line in `provenance.jsonl`. The last record for a path wins.

```json
{"path": "data/model/soldier0.bmp", "sha256": "…", "generator": "codex-cli 0.163 image_generation",
 "prompt": "…", "license": "CC0-1.0", "author": "…", "date": "2026-10-17", "curated_by": null}
```

`prompt` or `source_url` must be non-empty. All other keys must be present
(`curated_by` may be `null` until a human picks the asset).

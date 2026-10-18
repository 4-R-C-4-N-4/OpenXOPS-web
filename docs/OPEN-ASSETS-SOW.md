# Scope of Work: Fully open build (no original ZIP)

Status: approved direction, not started. Date 2026-10-18.
Builds on: FREE-DATA-PACK.md (pipeline/manifest), ASSET-CONTRACT.md (formats),
FINAL-POLISH.md (engine polish in flight).

## Goal

The public page boots straight into a playable game with zero downloads. Every
shipped byte is either BSD engine code or CC0/public-domain data with a
provenance record. The original xops096.zip stays an optional "classic data"
overlay for people who own it, never a requirement.

Licensing note: the engine source is BSD, but the original data (textures,
models, sounds, maps, mission scripts, briefing images) is not. We don't copy,
trace or derive from it. The private ZIP is used only to check that the engine
runs. Art workers never open it.

## What the engine actually needs (manifest: 353 slots)

Counted from assets/manifest.json (generated from the pinned upstream source):

| Kind | P0 (boot + 1 mission) | P1 (full roster) | P2 (all official missions) |
|---|---|---|---|
| Textures: HUD/font (char.dds, title.dds) | 2 | – | – |
| Textures: effects (blood/smoke/mflash/scope/yakkyou) | 5 | – | – |
| Textures: skies | 1 | 4 | – |
| Textures: character skins | 2 | 28 | – |
| Textures: weapon skins | 6 | 15 | – |
| Textures: small-object skins | – | 7 | – |
| Models (.x text, ≤~140 polys) | 27 | 25 | – |
| Sounds (22 kHz mono 16-bit WAV) | 14 | – | – |
| Maps (.bd1 block geometry + its tile textures) | 7 | – | 10 |
| Point files (.pd1: spawns, AI paths, events) | 8 | – | 57 |
| Mission text (.txt briefing, .msg in-game messages) | 1 | 2 | 114 |
| Briefing images | dir | – | – |

Current state: 15 generated (W1a tiles + some effects/skies on unmerged branches
fdp-w1b and fdp-sfx). Validator: 66 P0 failures.

Textures can come from Codex image generation, but most of the P0 work can't:
- Models: 52 .x meshes (humans, arms, weapons, small objects). Need a simple
  procedural/low-poly generator, not images.
- Maps: .bd1 is a list of 6-faced blocks with per-face texture and UV. Nothing
  in the repo writes it yet.
- Points: .pd1 holds spawn points, AI waypoints and mission events. Nothing in
  the repo writes it yet.
- The menu background is a live demo map (map10/temp.bd1 + op.pd1), so the
  title screen needs a free map too.
- The mission list is hard-coded: 58 official missions in src/parameter.cpp,
  pointing at original map directories. With no original data, most of those
  entries would point at files that don't exist.

## Phases

### A. Engine seams for open data (no art yet), ~2–3 days
1. Data-driven mission list: load missions from a pack file
   (e.g. data/missions.json, or reuse the addon .mif format, which the engine
   already parses). Keep the hard-coded table as fallback when the original ZIP
   is present. Hide entries whose files are missing.
2. Bundled pack: build step zips assets/pack into a fingerprinted
   openxops-data.<hash>.zip and mounts it via the existing VFS (directory/ZIP
   layering) before main. Layer order: free pack → optional original ZIP →
   personal overrides (IndexedDB). The page starts with no picker; the picker
   moves to "Load classic data (optional)".
3. Archive preflight and staging allowlist updated: the free pack is allowed to
   publish (CC0 hash list), original-asset MD5 guard stays.
4. Gate: page boots with an empty-but-valid pack (placeholder checkerboard
   textures, cube models, silent WAVs), shows the menu and starts a stub
   mission with no JS/engine errors.

### B. Authoring tools (enables everything after), ~3–5 days
1. tools/mapgen: .bd1 writer + reader round-trip test. Input is a simple text
   or JSON grid/block description. Output is a .bd1 plus a texture list.
   Validate by loading in the engine with collision.
2. tools/pointgen: .pd1 writer/reader for player spawn, enemies, weapons,
   waypoints, objective events. Round-trip tests against the open loader code
   (src/pointdata.cpp/datafile.cpp), not against original files.
3. tools/modelgen: emits text .x low-poly meshes (box-built soldier, arms,
   weapon silhouettes, crates) using UV layouts from ASSET-CONTRACT.md, so
   Codex-generated skins line up.
4. Gate: an authored test map with one spawn, one enemy and one exit event
   loads and plays natively and in the browser.

### C. Starter pack (P0): first original-free release, ~4–6 days
Content: 1 starter mission + menu demo map, 2 human skins, 6 weapons
(models + skins + HUD icons in char.dds), 5 effects, 1 sky, 14 SFX,
font/HUD atlas, title image, briefing text/image.
- Images: Codex image gen through tools/artgen (STYLE.md preamble), converted
  to required BMP/DDS by the existing tooling. Provenance logged per file.
- SFX: tools/sfxgen procedural synthesis (fdp-sfx branch, reviewed and merged
  first).
- Font: char.dds atlas rendered from a CC0/PD bitmap font, shared with the
  Japanese text work if its license allows.
- Gate: validate_pack.py has 0 P0 failures; the public page plays the starter
  mission start to finish with no ZIP; staging rejects any original hash; a
  Luna/Sonnet review of provenance passes.

### D. Full roster (P1), ~1–2 weeks of batch generation
Remaining 28 skins, 15 weapon skins, 25 models, 4 skies, small objects.
Mostly mechanical Codex batches + modelgen. All 23 weapons and 43 human
types become usable in open missions.

### E. Stretch: independent maps and new content
- A small campaign of new missions (not remakes of official layouts).
- Optional in-browser map editor built on mapgen/pointgen, since loose-file
  modding is a core goal.
- Pack import/export UI (FDP-UI) so players can share maps as ZIPs.
- Re-creating the 58 official missions is NOT in scope; their layouts are
  original content.

## Order and parallelism

A1/A2 and B1–B3 can run in parallel (different files). C needs A and B.
Art batches (C/D images) can start as soon as the formats are frozen, because
they only depend on the manifest and UV contract. Workers stay at three or fewer.
Low-cost models handle well-specified tool/test/batch tasks; the parent owns
integration, real browser play-through and deployment.

## Definition of done (open build)

- https://4-r-c-4-n-4.github.io/OpenXOPS-web/ plays with no file selection.
- Every shipped data file has a provenance line (generator, prompt/seed,
  CC0/PD) and no original-asset hash matches.
- Original ZIP still works as an optional overlay; personal overrides still work.
- README explains how to rebuild the pack from source: `make pack` or the
  equivalent script.

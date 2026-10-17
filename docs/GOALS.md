# OpenXOPS-web — Goals

## North star

Open a URL, and you're playing X operations-style FPS in the browser in under
10 seconds. No install, no upload, no cost to host. And it stays as moddable as
it was in 2006: every texture, model and sound is a plain file you can open,
edit and drop back in.

## Goals (in priority order)

G1. **Playable at a URL.** OpenXOPS 1.080 game logic, running as WebAssembly,
    hosted on GitHub Pages. Single-player.
    Spec: docs/SCOPE-OF-WORK.md (Phases 0–5).

G2. **Free to host, legally clean.** A complete Free Data Pack: original
    CC0 textures, models, sounds, maps and missions. The game runs from the
    pack alone, with zero bytes of X operations data.
    Spec: docs/FREE-DATA-PACK.md.

G3. **Moddable like the old days.**
    - Data is loose files in the original layout (`data/model/soldier0.bmp`, …).
    - Layered packs: Free Pack → (optional) the player's own `xops096.zip`
      → addons → "My edits".
    - In-browser: download the pack as a zip, edit it in any paint program,
      drag it back in, or drop one image onto a slot and see it live.
    - Edits persist locally and can be exported and shared.

G4. **AI-assisted asset production with Codex image gen.** A repeatable
    pipeline (`tools/artgen/`) turns a manifest slot plus a style bible into a
    game-ready, provenance-logged asset. Humans curate. The pipeline does the
    grunt work.

G5. **Native Linux build** as the debug target. It shares every backend with
    the web build.

## Non-goals (for now)

- Networked multiplayer (SOW §11 keeps the door open).
- Mobile and touch.
- Gameplay changes to upstream logic.
- Shipping, tracing or deriving from any X operations asset (NOTICE.md).

## Definition of done for v1

1. https://4-r-c-4-n-4.github.io/OpenXOPS-web/ boots straight into the title
   screen using the Free Data Pack.
2. At least one original map and mission is playable start to finish, with a
   full HUD, weapons, enemies and sound.
3. "Use my X operations data" (BYO `xops096.zip`) and "My edits" overlays
   both work.
4. Every shipped asset has a provenance line in `assets/ASSETS.md`.
   CI rejects any file whose MD5 matches the XOPS 0.96 file list.

## Workstreams

| Stream | Owner | Doc |
|---|---|---|
| Engine port (Phases 0–5) | subagents, orchestrated | SCOPE-OF-WORK.md |
| Asset contract + manifest + validator | subagent | FREE-DATA-PACK.md §3–4, ASSET-CONTRACT.md |
| Art pipeline (Codex image gen) | subagent | FREE-DATA-PACK.md §5 |
| Maps and missions | later | FREE-DATA-PACK.md §6 |
| Status | orchestrator | ../PROGRESS.md |

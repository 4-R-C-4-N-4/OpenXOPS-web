# PROGRESS

Goals: docs/GOALS.md · Engine plan: docs/SCOPE-OF-WORK.md (v0.2) · Data pack: docs/FREE-DATA-PACK.md · Asset contract: docs/ASSET-CONTRACT.md · Toolchain: docs/notes/toolchain.md

| Phase | Status | Notes |
|---|---|---|
| Fork + pristine import | done | GitHub fork of xops-mikan/OpenXOPS (README-only upstream; the source lives in SVN/zip). `src/` byte-identical to `1.080-src.zip`, tagged `upstream-1.080`. |
| 0 Repo & toolchain | done except CI | P0.1 UTF-8+LF conversion (round-trip verified, 63 files). P0.2 CMake native build with stub platform: gcc 16 / clang 22 under ASan+UBSan, 0 errors, exits cleanly; fixed 2 upstream UB sites. P0.3 emsdk 6.0.12 pinned, SOW §9 flags verified unchanged (web smoke test passes in Chromium). P0.5 DxLib sample diff not done. CI → Phase 4. |
| 1 Native Linux port | not started | Next. |
| 2 Emscripten target | not started | |
| 3 Web shell & data onboarding | not started | |
| 4 CI & GitHub Pages | not started | |
| 5 QA & polish | not started | |
| FDP asset contract | done | docs/ASSET-CONTRACT.md (239 citations against `upstream-1.080` line numbers), tools/contract/ (char layout JSON, template renderer, .x checker). |
| FDP manifest + validator | done | 353 slots (P0 81 / P1 81 / P2 191). Validator has a clean-room MD5 guard; 21 tests. |
| FDP art pipeline (Codex) | Wave 1a done | tools/artgen; 12 seamless CC0 map tiles in assets/pack/data/tiles_w1/ (preview: docs/img/w1a_tiles_preview.png). DDS + alpha output not built yet (needed for effects/HUD). |
| FDP-UI (edit/overlay UI) | not started | part of SOW Phase 3 |

## Decisions log
- 2026-10-17: "Online player" = browser-playable single-player at a URL. Multiplayer deferred (SOW §11).
- 2026-10-17: XOPS data is user-supplied (`xops096.zip`), stored in IndexedDB, and never hosted (SOW §3.1 C-2).
- 2026-10-17: Free Data Pack (CC0) is the default content, so the site hosts free and boots with no upload. BYO xops096.zip becomes an optional overlay. Codex CLI image_generation is the art generator; humans curate.
- 2026-10-17: Non-Windows backend ids are GRAPHIC_ENGINE 9 (GLES), SOUND_ENGINE 3 (OpenAL) and INPUT_INTERFACE 3 (SDL). Windows paths are kept behind `_WIN32` with their defaults unchanged. Key ids stay Windows VK codes (`src/platform/vkcodes.h`).
- 2026-10-17: Source literals are now UTF-8. gcc/clang therefore emit UTF-8 bytes for the Japanese ErrorInfo/log strings. An MSVC build of this tree needs `/utf-8` (or `/source-charset:utf-8 /execution-charset:shift_jis` to keep CP932 output).
- 2026-10-17: Upstream helpers that every backend needs moved verbatim to `src/platform/common.cpp` and `src/platform/input-common.cpp`. A Windows project must add both files.
- 2026-10-17: Pin Emscripten 6.0.12 (emsdk 1c9a998). SOW §9 emcc flags accepted as-is with no deprecations. CMake must override Release `-O3` → `-O2`.
- 2026-10-17: The manifest and asset contract are derived from the pinned `upstream-1.080` tag, not the working `src/`, so citations and slots stay stable while the port edits the source.
- 2026-10-17: No texture uses BlackTransparent (all 28 LoadTexture calls pass false). Font, title, scope and effects need a real alpha channel (DDS A4R4G4B4 / A8R8G8B8).

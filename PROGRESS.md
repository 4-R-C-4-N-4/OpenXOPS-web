# PROGRESS

Goals: docs/GOALS.md · Engine plan: docs/SCOPE-OF-WORK.md (v0.2) · Data pack: docs/FREE-DATA-PACK.md

| Phase | Status | Notes |
|---|---|---|
| Fork + pristine import | done | GitHub fork of xops-mikan/OpenXOPS (README-only upstream; the source lives in SVN/zip). `src/` byte-identical to `1.080-src.zip`, tagged `upstream-1.080`. |
| 0 Repo & toolchain | in progress | emsdk 6.0.12 installed + SOW §9 flag set verified unchanged (web smoke test passes in Chromium, native g++/clang++ OK). See docs/notes/toolchain.md. CMake skeleton pending. |
| 1 Native Linux port | not started | |
| 2 Emscripten target | not started | |
| 3 Web shell & data onboarding | not started | |
| 4 CI & GitHub Pages | not started | |
| 5 QA & polish | not started | |
| FDP manifest + validator | not started | |
| FDP art pipeline (Codex) | not started | Codex image gen smoke-tested OK (1254² PNG, ~60 s, seamless tile). |
| FDP-UI (edit/overlay UI) | not started | part of SOW Phase 3 |

## Decisions log
- 2026-10-17: "Online player" = browser-playable single-player at a URL. Multiplayer deferred (SOW §11).
- 2026-10-17: XOPS data is user-supplied (`xops096.zip`), stored in IndexedDB, and never hosted (SOW §3.1 C-2).
- 2026-10-17: Free Data Pack (CC0) is the default content, so the site hosts free and boots with no upload. BYO xops096.zip becomes an optional overlay. Codex CLI image_generation is the art generator; humans curate.
- 2026-10-17: Pin Emscripten 6.0.12 (emsdk 1c9a998). SOW §9 emcc flags accepted as-is with no deprecations. CMake must override Release `-O3` → `-O2`.

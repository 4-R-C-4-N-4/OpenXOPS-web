# PROGRESS

Plan: docs/SCOPE-OF-WORK.md (v0.1). Phases and gates (★) are defined there.

| Phase | Status | Notes |
|---|---|---|
| Fork + pristine import | done | GitHub fork of xops-mikan/OpenXOPS (README-only upstream; the source lives in SVN/zip). `src/` byte-identical to `1.080-src.zip`, tagged `upstream-1.080`. |
| 0 Repo & toolchain | not started | |
| 1 Native Linux port | not started | |
| 2 Emscripten target | not started | |
| 3 Web shell & data onboarding | not started | |
| 4 CI & GitHub Pages | not started | |
| 5 QA & polish | not started | |

## Decisions log
- 2026-10-17: "Online player" = browser-playable single-player at a URL. Multiplayer deferred (SOW §11).
- 2026-10-17: XOPS data is user-supplied (`xops096.zip`), stored in IndexedDB, and never hosted (SOW §3.1 C-2).

# PROGRESS

Goals: docs/GOALS.md · Engine plan: docs/SCOPE-OF-WORK.md (v0.2) · Data pack: docs/FREE-DATA-PACK.md · Asset contract: docs/ASSET-CONTRACT.md · Toolchain: docs/notes/toolchain.md

| Phase | Status | Notes |
|---|---|---|
| Fork + pristine import | done | GitHub fork of xops-mikan/OpenXOPS (README-only upstream; the source lives in SVN/zip). `src/` byte-identical to `1.080-src.zip`, tagged `upstream-1.080`. |
| 0 Repo & toolchain | done except CI | P0.1 UTF-8+LF conversion (round-trip verified, 63 files). P0.2 CMake native build with stub platform: gcc 16 / clang 22 under ASan+UBSan, 0 errors, exits cleanly; fixed 2 upstream UB sites. P0.3 emsdk 6.0.12 pinned, SOW §9 flags verified unchanged (web smoke test passes in Chromium). P0.5 DxLib sample diff not done. CI → Phase 4. |
| 1 Native Linux port | in progress (wave 2) | Parallel branches: p1-shell (SDL window/input/main loop + OpenAL), p1-gles (GLES3/WebGL2 renderer), p1-vfs (layered VFS + zip), p1-text (CP932 + Shinonome glyph atlas). Baseline: game logic runs 300 ticks on real XOPS data (stub platform) under ASan+UBSan, clean. |
| 2 Emscripten target | not started | |
| 3 Web shell & data onboarding | not started | |
| 4 CI & GitHub Pages | not started | |
| 5 QA & polish | not started | |
| FDP asset contract | done | docs/ASSET-CONTRACT.md (239 citations against `upstream-1.080` line numbers), tools/contract/ (char layout JSON, template renderer, .x checker). |
| FDP manifest + validator | done | 353 slots (P0 81 / P1 81 / P2 191). Validator has a clean-room MD5 guard; 21 tests. |
| FDP art pipeline (Codex) | Wave 1a done; W1b paused (Anthropic limit); SFX complete and reviewed (14 procedural WAVs, 3 tests; listening/mix approval pending) | tools/artgen; 12 seamless CC0 map tiles in assets/pack/data/tiles_w1/ (preview: docs/img/w1a_tiles_preview.png). DDS + alpha output not built yet (needed for effects/HUD). |
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
- 2026-10-17: OQ-1 resolved → Shinonome bitmap fonts (public domain). FDP-3 resolved → SFX are procedurally synthesised (CC0, reproducible).
- 2026-10-17: Build is split per component (`OPENXOPS_{WINDOW,INPUT,SOUND,GFX}_BACKEND`, `cmake/modules/*.cmake`). Window↔renderer contract is `src/platform/gl_present.h`.
- 2026-10-17: Original xops096.zip (MD5 verified) is kept at `xopsdata/` (gitignored) for local engine testing only. Art/SFX agents are forbidden from opening it (clean room).

## Recovery status

Anthropic wave 2 stopped after HTTP 429. Partial files remain in the component worktrees; none of the incomplete engine backends are merged. The sound branch was recovered on openai-codex/gpt-6.1-sol with low reasoning, independently reviewed, merged, and all 24 Python tests pass. Delegation is configured for that provider/model with low reasoning; Luna model ID is not yet identified.

## Active recovery batch

Implementation resumed with at most three concurrent workers on
`openai-codex / gpt-6.1-sol`, reasoning effort `low`. This is a verified
working provider/model, not a claim that it is the cheapest available model;
no Luna alias has been identified. Tasks are deliberately smaller to reduce
context and failed-work costs. No Anthropic requests.

| Branch | Active scope | Gate |
|---|---|---|
| p1-vfs | Recover existing VFS library; layered dir/ZIP mounts, safe CP932 paths, save precedence, tests. Game-call-site integration follows review. | Sanitized build + unit tests + real ZIP CRC/decompression test |
| p1-shell | SDL window and keyboard/mouse only; GLES context/presentation interface. OpenAL and main-loop restructuring are separate follow-ups. | Offscreen and dummy-driver bounded runs + input tests |
| fdp-w1b | Recover DDS writer, add alpha processing, preserve BMP output, generate procedural scope without duplicate reticle. No image-generation calls in this task. | DDS known-pixel tests + existing Python tests + scope validator |

Paused: GLES renderer and Japanese text branches. Next sequence: review and
merge these bounded components, integrate VFS reads, then renderer and text.
The browser game is not playable yet. Completed sound pack remains pending
human listening/in-engine mix review.

### Recovery batch results (awaiting review)

- p1-vfs `4b5e950`: library complete; parent reran CTest 2/2, including 497 original archive files read/CRC-checked. Game integration still deferred.
- p1-shell `065b1d0`: SDL window/input complete; backend and full-game headless/offscreen smoke tests provided. Renderer/audio still stubs.
- fdp-w1b `2b37a3a`: DDS/alpha tooling + procedural scope complete; parent reran 6 tooling tests successfully. Scope leaves 66 required P0 slots missing.
- Three independent read-only reviewers now run on `openai-codex/gpt-6-luna` with low reasoning. Runtime worker list confirms that exact model. Results are not merged until review and integration pass.

### Review corrections in progress

Luna reviewers withheld approval. Three bounded Luna correction tasks are active:
- VFS: reject invalid above-root read paths and validate ZIP directory names before skipping them; add regression coverage. Trusted configured save-root boundary must be explicit.
- SDL: investigate relative-motion consumption across absolute/relative polls, with regression tests and protection against stale menu movement.
- DDS: reject either zero dimension and preserve alpha context in contact sheets. Verify manifest-format concern against canonical `build.py` (which also uses indent=1) before changing it.

These are findings under verification, not claims that every reported issue is confirmed. No affected branch has been merged.

### Integrated recovery gates

- VFS fixes independently re-reviewed APPROVED; component merged. Parent rebuilt sanitized main and ran both VFS tests successfully (including real archive).
- DDS/alpha fixes merged; parent ran all 32 Python tests (21 manifest/validator + 8 artgen + 3 sound), all passing. Follow-up read-only check pending. Scope remains generated, not human approved.
- SDL remains on branch pending final re-review. Parent resolved the alleged missing-data blocker: local original data IS present at main checkout's `xopsdata/extracted`. Fresh full-game SDL build succeeded with explicit stub graphics/sound; 100 ticks each offscreen/dummy exited cleanly under sanitizers. Both SDL CTests passed. No stale-menu-delta accumulation: pump-boundary reset now implemented/tested.
- Browser renderer and VFS game-call integration remain next; current main still uses stub rendering.

### Next implementation batch

SDL input and DDS follow-up Luna reviews both APPROVED. SDL merged; parent
rebuilt the integrated sanitized SDL+VFS configuration: CTest 4/4 and full
game 100 ticks each under offscreen/dummy passed. Rendering/audio remain stubs.

Three low-reasoning Luna tasks now active (cap 3):
- `vfs-integrate`: route game reads/startup/addon listing through VFS; prove ZIP-only startup without extracted data.
- `audio-openal`: real SoundControl backend, robust PCM WAV handling, VFS reads, null-driver tests using our generated sounds.
- `gfx-core`: matrix math, GLES3/WebGL2 shader/VBO device, synthetic offscreen scene and pixel assertions. This is a tested foundation, not yet the full D3DGraphics replacement.

Japanese text and the full D3DGraphics method port follow these gates. No claim
of playable graphics or browser deployment yet.

### ZIP/audio/GLES batch review

- ZIP-only VFS integration committed `18c663f`; worker reports clean 100-frame ZIP-only startup. Independent Luna review active before merge.
- OpenAL implementation remains uncommitted: parent caught allocation of 100 voices per sound (up to 2400), contrary to shared 100-voice design. Correction task active, requiring all 14 generated sounds loaded concurrently and full-game smoke with the known absolute local data path.
- GLES foundation remains uncommitted: initial synthetic harness is incomplete and full CTest has a 112-byte driver-side leak report. Completion task requires textured cube/lines, texture/alpha/fog/MVP pixel assertions and clean sanitizer gate (not blanket leak disabling).
- These partial implementations are not merged or described as complete. All active agents use Luna/low reasoning, cap 3.

### Direct verification after completion gaps

Parent directly rebuilt OpenAL configuration: CTest 4/4 and real-data100tick
offscreen/dummy smoke both pass with null audio driver and sanitizers. Final
shared-pool review active. Backend still uncommitted pending review.

VFS integration reviewer raised a Windows-forward-slash objection; that is
not an established blocker (Windows CRT supports forward slashes and CMake
is explicitly non-Windows). A second factual integration review is active.

Parent reproduced GLES failure: every sampled pixel stays background, so
rendering is not yet verified. A narrow Luna task is debugging that actual
failure instead of papering over pixel assertions or sanitizer settings.

### ZIP-only integration merged and verified

VFS game integration re-review APPROVED. Parent merged and fixed an integration
link omission: SDL shell test compiles debug.cpp, which now needs openxops_vfs.
Combined sanitized CTest4/4 passes. Parent ran game100ticks from original ZIP
with an EMPTY runtime directory: exit0, no physical data/ directory, no
sanitizer report. Renderer remains stub, so this is data-loading verification.

Audio left-channel selection is intentional original-compatibility behaviour,
not a stereo-preservation bug; a sample-content regression is being added.
GLES harness inspection found malformed9-float vertex records and wrong
transformed sample coordinates; targeted correction task active.

### Audio test correction and graphics status

Parent found initial audio test never called SoundControl::InitSound, so the
old playback assertions only exercised silent mode. Fixed before merge:
backend initialized on null driver, exact synthetic stereo left-channel bytes
asserted, decoder allocation bounded64MiB, helper extraction compiles.
Committed audio branch d33c2d5. Sanitized4CTest +100ticks offscreen/dummy pass;
final independent review pending. Intentional stereo-left compatibility kept.

GLES harness now checks actual blue texture, alpha discard, white texel, half
fog purple and yellow line successfully. Parent confirmed full leak-enabled
CTest STILL fails112bytes in2driver allocations. It is not merged. A narrow
Luna diagnostic is identifying the external stack; blanket leak disabling
remains unacceptable. Harness currently uses triangles/line, not a cube, and
clip-depth fog is not yet production camera-distance fog.

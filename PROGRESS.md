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

### Upstream-grounded audio resolution

The latest review requested spatial pan, but upstream sound-ezds.cpp explicitly
comments out pan calculation and uses0; volume0 is midrange, not silence
(valid range -100..100). Parent kept these intentional semantics and corrected
a real attenuation mismatch to upstream CalculationVolume, including335unit
boundary. Branch42820b2 tests exact positive/negative attenuation values and
stereo left samples with initialized real OpenAL-null playback. Sanitized4CTest
passes; final narrow review compares upstream formulas, not redesign requests.

GLES five pixel checks pass but112byte driver leak remains unresolved after
symbolization attempts; foundation remains unmerged and no further Luna retry
on that leak is active. Do not claim graphics sanitizer gate passed.

### Audio integrated

Upstream-formula review APPROVED. OpenAL backend merged into main. Parent
fresh sanitized SDL+VFS+OpenAL/stub-renderer build: CTest5/5 passes, 100ticks
each offscreen/dummy passes, and ZIP-only empty-directory100tick run with
OpenAL null driver passes. This verifies decoder/playback API and loading,
not human-audible mix quality. Main still has no real game renderer.

Completed native platform components: SDL window/input, layered VFS with game
integration, OpenAL audio. Remaining core port: D3DGraphics rendering,
Japanese font text, browser mainloop/persistence shell and deployment.

### Boot instructions and browser readiness

README now states plainly that the browser game is not yet playable and links
to docs/RUNNING.md. Parent rebuilt browser toolchain smoke with emsdk6.0.12,
served it locally, verified Chromium summary120frames/gl=yes/failures0 and
IDBFS run counter1->2 across reload. Local preview server serves only
`tools/toolchain-smoke/build-web` at127.0.0.1:8765.

First playable browser preview is explicitly BYO-original-data, independent of
free-pack completeness. Remaining: full game renderer, Japanese font path,
Emscripten game build/mainloop and native VFS portability, browser archive
onboarding/save/audio/pointer-lock. Native-only POSIX library passing is not
proof of web compatibility. No public playable deployment exists.

## Critical-path reset: actual playable browser mission

User explicitly rejected triangle/demo progress as the deliverable. Supporting
art production, speculative parity work and isolated native driver-leak
investigation are deferred. Success means: upload original local archive,
reach title/menu, launch a mission, visibly render map/actors/HUD, respond to
movement/fire, with a real browser screenshot and console verification.

Active three Luna/low workers:
- playable-render: adapt COMPLETE upstream OpenGL renderer rather than another draw-device foundation; fixed-function Emscripten emulation is acceptable for first preview. Temporary ASCII font fallback allowed.
- playable-web: compile actual game WASM, browser mainloop, VFS portability, local ZIP upload and pointer/audio gestures. Original assets never served or committed.
- playable-text: minimal CPU bitmap Japanese/ASCII path for integration; must not block first mission if ASCII fallback works.

Parent will merge components and test the actual game in browser. Existing
triangle smoke is not acceptance evidence for this milestone. No promise of
playability until mission rendering and controls have been exercised.

## Working browser gameplay preview

Milestone: original game now boots locally in Chromium through Emscripten
LEGACY_GL_EMULATION and an adapted upstream GL backend. Shader rewrite is
no longer a prerequisite for first play. Build: tools/build-playable-web.sh;
serve only build-web at localhost8766; player selects own xops096.zip.

Parent verified real opening -> mission menu -> Training Yard briefing ->
first-person mission, original textures/models/HUD, keyboard look response and
~30FPS. Rebuilt after guarding native SwapInterval browser timing warning;
second startup reaches Game running with only expected GL-emulation warning.
Native regression CTest5/5 passes. README/docs/RUNNING.md now document the
GAME preview and original controls, not the triangle demo.

Limitations: Japanese text uses '?' placeholder, some title/UI artifacts,
archive/settings not persisted, no complete mission/audio/performance parity
claim. No original assets or derived screenshots shipped. Independent Luna
critical-runtime/privacy review pending; local server running for user testing.

Browser preview Luna review completed: no critical runtime/privacy blocker found. Local archive is read into browser memory/VFS only; no upload endpoint or asset publication path. Static screen lifetimes and browser-yielding loop reviewed. Japanese placeholder text remains a known limitation.

### Archive-selection startup fix

Reproduced exact missing-data error by selecting Downloads/1.080-src.zip;
old launcher falsely showed Game running. This reproduces the user's symptom,
not proof of which ZIP they selected. Added real VFS/CRC archive preflight and
required representative game files before main, descriptive missing-data errors,
retry without restarting, original-game download link, and startup exit/error
handling. Chromium: source ZIP rejected with explanation and retry enabled;
selecting verified xops096.zip on same page then boots original opening scene.
Three synthetic regressions (wrong->correct retry, wrapping folder, corrupt ZIP)
and existing21tests pass (24 total). WASM rebuilt; RUNNING.md explains archives.

### Fingerprinted preview deployment and actual Downloads archive test

User reported missing Module._openxops_prepare_archive: served engine currently
exports it, consistent with new HTML plus stale cached unversioned JS. Added
shared JS/WASM content fingerprint filenames, explicit locateFile mapping,
atomic page packaging and startup API guard. No longer copy raw HTML on edits.
Two regression tests verify fingerprint changes for either binary and preserve
working page if template invalid. Parent tested EXACT /home/ivy/Downloads/xops096.zip
in Chromium with fingerprinted script3adad08bbb6b42a9: preflight function exists,
opening renders, Enter menu, Training Yard click/briefing, gameplay, arrow look
and W key state verified; no missing-function/data-dir error. User archive valid.

### Play-view layout fix

Replaced scrolling setup+fixed960px canvas with fixed full-window play stage,
centred4:3 viewport-fit scaling and fullscreen toolbar. Renderer backing buffer
and GL viewport remain640x480 (CSS-only resizing avoids high-DPI corner renders).
Errors restore setup view. Parent tested exact Downloads/xops096.zip: opening
visible/centred at1280x577, fullscreen button succeeds, resized1600x1000 atDPR2
fits1269x952 with640x480 GL viewport and no clipped canvas. Build repackaged via
fingerprint tool; original archive remains local.

### Screenshot-rooted tiny-corner fix and archive persistence

User screenshot xops-web-tiny.png showed actual image in lower-left, not just
page misalignment. Parent reproduced by forcing3832x1904 backing buffer while
GLviewport stayed640x480. Portable StartRender now restores backing buffer to
logical game size before rendering and refits CSS. Exact same resize now
self-recovers to640x480/full displayed game. Tested1920x1080/DPR2 too.

Validated ZIP now saved locally in IndexedDB after preflight, loaded before Start
button enabled, and usable without picker after reload. New invalid selection
never overwrites saved valid archive. Forget button removes persisted record;
storage failure degrades to normal picker. No cookies or uploads. Parent tested
real DownloadsZIP save->reload->Start(no file),forget->reload(no record),resave;
all passed in Chromium. Settings persistence remains separate/unimplemented.

## Minimal demo live on GitHub Pages

Live: https://4-r-c-4-n-4.github.io/OpenXOPS-web/
Portfolio Games cabinet added in root-site commit920ee5f, embedfalse and
standalone url; existing5manifest games remain intact. Browser click verified
new tab at project URL. Root Pages deployment39134293797 succeeded.

Engine deployment39134157979 succeeded (commit7e5c875). Independent project
Pages workflow builds on source/web/tool changes, pins emsdk6.0.12, runs26tooling
tests and uploads allowlisted page/fingerprintedJS+WASM/LICENSE/NOTICE only.
Downloaded actual artifact inspected: no original archives/assets or local data.
Public Chromium test used exact Downloads/xops096.zip: opening, menu,briefing,
Training Yard gameplay; cache record saved at public origin. Relative engine
asset URLs work at /OpenXOPS-web/ and fullscreen viewport remains640x480.

Root site deployment remains branch-based; tiny-games workflow/manifest left
unchanged. Future OpenXOPS pushes deploy independently without root-site edits
or crossrepo secrets. See docs/DEPLOYMENT.md and README live link.

## Rendering cleanup and accessible controls — active

Plan: docs/RENDERING-CLEANUP-PLAN.md. Baseline d3ebefd;26tooling tests pass.
Three low-reasoning gpt-6-luna workers, isolated worktrees, cap3:
- cleanup-reticles: reproduce/fix scoped PSG1 pixel path; only owner of active
  portable renderer. Unscoped PSG1 absence is intentional and stays unchanged.
- cleanup-assets: source/ZIP audit + synthetic tests + missing-asset issue ledger;
  distinguish held/Home-disabled weapons, HUD models and world models. No shared
  renderer edits in this task.
- cleanup-controls: accessible key guide and explicit Modern/Classic setup preset,
  guarded local preference, dialog focus/pointer-lock and resume behaviour.

Current live preview stays unchanged until local runtime gates and independent
review pass. Parent will integrate, rebuild packagedWASM and verify actual
repaired states; source-only reviews do not establish rendered correctness.

### Cleanup first-pass review

Controls branch03c261d implemented UI/presets but no actual selected-ZIP gameplay
test. Parent found guide errors (Home notTab forarms; ClassicShift notrightmouse
foraim) and preset mismatch whenlocalStorage fails. Followup task correcting
these and testing dialog pause/resume with inputflush and real DownloadsZIP.

Asset-audit draft only scannedliteralLoadcalls, missingdynamicweaponparameter
paths; followup adds realweapon-tablecoverage,syntheticregressions and actual
archiveaudit. No rendererchanges accepted fromthisstream.

Reticle worker didnotproducepixelrepro and onlyadded End2DRenderalphaenable,
not its claimeddisable; blackalpha1 vsGREATER0 explanationunproven. No merge.
Followup requires productionline/box pixelproof beforeanyreticlefix.
Correction pass completed; parent performed the missing runtime gates below.

### Parent-verified cleanup slice (local integration)

- Corrected guides AGAIN: Home arms; Classic Shift scope and Tab walk. Modern
  now has right-mouse scope, independently from Shift walk. Optional, saved
  preset; Classic defaults preserved. Storage denial retains page-session choice.
- Actual Downloads/xops096.zip: Training Yard, Modern W movement, help pauses
  simulation, key release while help open, Escape/focus restoration, no persistent
  movement after resume. Fullscreen help visible; resume renders full scene with
  640x480 backing. Production build has no QA exports.
- Parent built diagnostic seam explicitly behind OPENXOPS_TEST_HOOKS=1. Production
  Draw2DLine/Box test returns 360/360 dark row pixels with fog off AND on.
  Actual PSG1 scoped crosshair visibly works against sky in unmodified renderer.
  All22 normal weapon model/texture handle pairs valid; Home-on held geometry
  and HUD models visibly render forMP5,PSG1,M4,AK47,M249. No speculative alpha fix.
- Public staging refuses QA engine BEFORE touching an existing destination;
  normal production build and allowlisted staging pass.31 tooling tests pass.
- New reproducible protocol: docs/RENDERING-BUGS-RETICLES.md. Original-derived
  screenshot evidence stays in private browser workspace, notrepo/public files.
- Asset audit integrated after comment-aware correction: parent reran against
  tracked C++ and exact Downloads ZIP.92 references:90 present,2 dynamic paths
  unverified,0 missing,0 empty.23 weapon slots include intentional empty NONE;
  all22 normal model/texture pairs present. Full JSON stays in private scratch.
- Combined tooling suite35 tests passing; no engine/renderer change from asset
  audit. Independent review approved with no concrete release blockers.
- Published7157280 through successful Pages run39139280261. Parent verified live
  fingerprint16bda66006a22893, no QA exports, exact Downloads ZIP through Training
  Yard, corrected Modern guide, and help pause/focus at the public origin.
  Live: https://4-r-c-4-n-4.github.io/OpenXOPS-web/
- Remaining parity work notdeclared complete: other maps/assets and Japanese text
  questionmarks; no claim that allvisualbugs arefixed by controls help.

### Final polish before original-free starter pack — active

User approved clearing remaining refinements before custom graphics. Scope:
docs/FINAL-POLISH.md. Three isolated workers verified running gpt-6-luna:
- polish-text: real Japanese text, CP932 boundaries, freely redistributable font,
  actual menu/briefing rendering; sole renderer owner.
- polish-lifecycle: explicit input-safe pause/resume on blur/hidden, compose with
  Controls help and fullscreen without unintended firing or stuck movement.
- polish-audit: concrete remaining release issues and validator-backed smallest
  original-free starter-mission gate; no art generation or original asset study.
Parent owns integration, independent review, exact-ZIP browser regression and
live verification. Existing public build stays unchanged during this work.
Replacement pack must cover maps/points, models, audio and UI as well as textures;
CC0/public-domain provenance required, not merely absent licensing notices.


### Open-assets overhaul (no original ZIP) — scoped, not started

Scope: docs/OPEN-ASSETS-SOW.md. 353 manifest slots; P0 = 2 HUD/font, 5 effects,
1 sky, 2 skins, 6 weapon skins, 27 models, 14 SFX, 7 maps + 8 point files +
mission text (incl. menu demo map). Images via Codex image gen; models, maps and
points need new tooling (modelgen, mapgen .bd1, pointgen .pd1).
Phases: A engine seams (data-driven mission list, bundled fingerprinted free pack
mounted via VFS, ZIP becomes optional overlay) → B authoring tools → C P0 starter
pack = first upload-free public release → D full roster → E stretch: new maps,
campaign, in-browser editor. Official mission layouts are not remade.
Starts after the final-polish batch is integrated.

### Final polish integration (subagents now anthropic claude-sonnet-5)
- Codex 429 killed all three Luna workers; re-dispatched on Sonnet.
- Lifecycle merged: pause reasons (dialog/blur/hidden), input flush, Resume button.
  Parent fix: regaining focus/visibility no longer auto-resumes (worker version
  did, which let held W leak). Parent-verified in Training Yard with exact ZIP:
  idle stable, held W + blur + release + focus stays paused, explicit Resume → no
  drift; dialog close keeps blur pause.
- Audit merged: docs/POLISH-AND-FREE-PACK-GATES.md (66 P0 missing; fdp-sfx/w1b
  already merged).
- Text: polish-text 9a817a7 has CP932 decode/measure/render pipeline but atlas has
  only Latin-1, so Japanese shows boxes. Follow-up worker generating Shinonome
  JIS X 0208 glyph atlas; not merged until legible text verified in browser.
- Text merged (polish-text 11a25a9 + parent fix): Shinonome PD atlas, 7,028 glyphs
  (JIS X 0208 L1+L2, half-width kana, Latin-1), WASM +230 KB. Parent found CRLF
  briefing lines drew tofu boxes for '\r'; C0 controls now zero-width (+test).
  Verified Training Yard briefing renders "訓練せよ / (説明の操作方法は標準設定の場合)"
  legibly with exact ZIP. 47 tests pass. Final polish batch complete; next: Phase A
  of docs/OPEN-ASSETS-SOW.md.

### Open-assets Phase A + B — active (Sonnet workers, isolated worktrees)
- open-engine: bundled deterministic free pack ZIP mounted via VFS, original ZIP
  as optional overlay, data/missions.txt mission list with official fallback,
  placeholder pack mode so the page boots to a stub mission with no original data.
- open-mapgen: clean-room .bd1/.pd1 reader/writer from src/datafile.cpp, JSON
  author format, original starter layout, round-trip + real-loader harness.
- open-modelgen: procedural text .x generator for P0 model slots + UV templates
  for Codex skin painting.
Parent integrates, then wires mapgen/modelgen output into the pack and verifies
an upload-free mission in browser before any deploy.
- Merged: mapgen (e4718ae; real loader reads starter as 7 blocks/6 points; 27 tests)
  and modelgen (de2e4f7; all 27 P0 models, 0 model-slot validator failures).
  Validator P0 failures 66 → 39 (textures/maps/sounds remain).
- open-engine was partial: only tools/build_pack.py kept (its placeholders wrote
  into assets/pack — discarded; must stage separately). Re-dispatched as
  open-engine2 with full Phase A spec. Parallel: open-starter authors original
  starter compound mission, menu demo map, missions.txt and a Codex texture list.
- Phase A done by parent (7f649e5) after two partial worker attempts: free pack
  bundled (fingerprinted, placeholders staged outside assets/pack, zip-member
  original-MD5 guard), VFS layering free→classic, data-driven mission list
  (docs/MISSIONS.md), page Start works with no file. Browser-verified: no file →
  opening + menu (free map10 demo) + STARTER mission loads; with xops096.zip the
  classic menu/58 missions unchanged. 125 tests pass. NOT deployed yet.
- Blockers before deploy: STARTER ends instantly with no movement (worker
  open-fix root-causing); placeholder char.dds makes all text unreadable (worker
  open-font generating CC0 atlas from Shinonome).
- 878f8de: merged starter team fix (mapgen now emits HUMANINFO; ab6ae9c) and CC0
  char.dds/title.dds atlas (a5ac14a). Parent fixes: placeholder generator no longer
  stubs maps/points (stubs made 58 fake classic missions appear), main menu demo
  falls back to free data/menu/ when classic demo maps are absent, build cleans old
  pack zips. Browser (no file): opening → menu shows readable text and only
  STARTER → mission starts. Found: player falls through floor and dies, weapon NONE.
  Note: blur pause blocked earlier workers' synthetic input — click Resume first.
- Active: open-fix2 (floor collision/spawn/weapon), open-tex (Codex P0 textures).

### Rate-limit recovery and CI repair

Both Sonnet workers stopped on Anthropic 429s. Parent checked live delegation:
none remained running. Preserved uncommitted work in open-fix2 and open-tex;
re-dispatched bounded recovery tasks and verified both live models are
`gpt-6-luna` via `openai-codex`. Delegation defaults updated with Hermes CLI.

Failed Actions run39155547977: `test_char_atlas` could not import `PIL` because
Pillow was installed locally but undeclared/uninstalled in CI. Fix9ecc53c in
PR https://github.com/4-R-C-4-N-4/OpenXOPS-web/pull/1 adds Python3.12 setup,
pinned tools/requirements-ci.txt (Pillow12.3.0), and asset/test path triggers.
Fresh isolated Python3.12 environment:133 tests pass. Real branch Actions
run39159423647 succeeded through tests, WASM build, free-pack staging/original
hash guards and artifact upload; deploy deliberately skipped for non-main refs.
PR held pending actual STARTER ground/movement/weapon regression gate: merging
CI onto main would automatically publish the still-unverified free starter.
No live-game replacement claimed. Parent owns merge/deploy after recovery gates.

### Free-starter recovery integration — release candidate

Luna recovered9c65f327 (loadout/collision harness) and96e79b1 (16 map/sky texture
files and validated provenance). Parent integrated them with the tested CI fix
on isolated branch integrate/free-starter, not main.

Parent browser QA found the real immediate-death cause: initial HP110, M92F
present; enemy hits reduced HP19 then-64 in about1second while feet remained
y0. NONE was a death/drop symptom. Generated walls5/cover2 were undersized
against HUMAN_HEIGHT20. Rescaled source geometry/points8x and added full-height
spawn cover; rebuilt source-authored BD1/PD1 and refreshed provenance hashes.
Removed speculative SDL mode-transition patch (original input retained).

Verified local production artifact, no selected ZIP and no QA exports:
- STARTER alive/unpaused after30seconds, then W moved z159.92→154.72 with
  ground-level y; M92F fired (ammo reduced). QA build separately retainedHP110.
- Exact Downloads/xops096.zip overlay still loads Training Yard; W movement
  and original textures/HUD verified.
-137 tests pass, including actual engine collision harness and map-scale tests.
  Production WASM build and allowlisted staging/original-hash guards pass.
- README/RUNNING now describe no-file STARTER and optional classic data.
Release review resolved: original pickup selects an empty slot (1/2), not an
occupied slot. Real browser QA read slots[3,-1]→[3,3] on key2; a C++ test runs
the exact production PickupWeapon body and verifies a different second weapon
without replacing the first. No auto-pickup redesign. Fixed a buried spare
pistol (spawn barricade overlap), changed indoor MP5 to included M4, and added
model-presence / unobstructed-pickup tests. Updated guide and briefing.
Canonical provenance now has one current record per path, correct rebuilt
hashes and pinned source URLs; no provenance/known-original-match failures.
The legacy all-missions validator still fails33 missing-P0 checks, expected
for the unfinished starter profile (classic demo maps and placeholder skins/
effects); not reported as a complete353-slot pack.142 tooling tests pass.
Production build/staging passed with no QA exports. Final release checks follow.
This is an unfinished free starter prototype, not all353 asset slots or final art.

### First no-ZIP public starter — deployed and verified

PR #2 merged at aa0cb90d6f5aa045913d64486014947fafd03b50; CI fix PR #1
was included and is also marked merged. Main Actions run 39161129776 passed
142 tests, WASM build, pack staging and Pages deployment.

Parent exercised the public URL in an isolated clean browser with no selected
or remembered original ZIP: opening -> STARTER briefing -> actual gameplay;
30 seconds alive/unpaused, ground-level W movement, empty-slot pickup and M92F
firing. Verified loaded engine d21d1fc40c59f582 and data pack
065f76408b36ef23, with QA exports absent. A separate public-origin test loaded
the exact Downloads/xops096.zip and verified Training Yard textures/movement.

Live: https://4-r-c-4-n-4.github.io/OpenXOPS-web/
Existing users with a remembered classic ZIP can use Forget saved ZIP to try
the bundled starter. Original classic data remains optional and browser-local.

Remaining: replace procedural placeholder skins/effects, extend free weapon/
character coverage, refine mission balance/patrol/objective behavior and verify
a full victory play-through. Do not call the full free-data-pack SOW finished.

### User-reported visual defects — corrective pass active

User reports checkerboard people and incorrect geometry. Parent reproduced the
live free pack and found concrete asset issues, not just an unfinished style:
- Active human skins are missing and replaced by checkerboard build fallbacks.
- Generated upper arms and forearms are coplanar: every vertex has y=-0.02.
  `_one_arm` used the Y-oriented tapered_box with equal base/top Y and differing
  Z. Numeric mesh bounds confirm zero vertical thickness.
- Starter world was rescaled, but menu/opening demo still has a 36-unit floor
  and 5-unit walls around 20-unit-tall people. Separate demo correction needed.
- Starter briefing requests sky0; the engine accepts only1..5. Black background
  is not evidence the generated sky1 texture was actually displayed.
Three isolated Luna workers: visual-geometry (volumetric meshes/joints/tests),
visual-skins (actual active UV-aligned human skins, Codex image generation),
visual-map (demo scale/framing, sky, world texture density). UV rectangle
contract frozen between mesh and skin streams. Parent owns provenance updates
and combined close-up/browser review before publication. No new deployment yet.

Map worker returned an uncommitted partial. Parent completed the map component
on visual-map (f0be894): eight-times demo geometry/points, synchronized menu
copies, valid starter sky1, per-face texture_world_units option so tileable
surfaces do not stretch one image across a whole courtyard, four regressions,
source/binary/hash provenance refreshed.146 tests pass and WASM builds.
Local browser confirms clouds appear instead of black sky. Skybox face seams
remain visible; not declared visually finished. Character skins/meshes are still
separate pending streams. Branch pushed for reproducible source links, NOT merged
or deployed. Combined character and map visual acceptance remains required.

### Visual integration — integrate/visual-polish (parent + Sonnet workers)

Branch `integrate/visual-polish` merges visual-map, visual-geometry and
visual-skins, plus parent fixes found during engine-composed close-up review:
- Geometry: `tapered_box` cross-sections follow the limb axis (flat-arm root
  cause). Arms rebuilt shoulder -> elbow -> wrist with hands landing on the
  weapon grip (two-hand rifle, one-hand pistol). Wider torso, neck, cube head,
  soles on the ground transform, weapon length matched to the engine muzzle
  anchor in specs. Regressions: tools/tests/test_human_assembly.py.
- Skins: Codex-material UV-aligned soldier_green/black. Fixed BMP palette
  written RGBA instead of BGRA (red/blue swapped); regression added. Face,
  hair, skin-tone panels.
- Sky: root cause of seams = one continuous picture cut into six box-face
  quadrants. New tools/artgen/skybox.py renders a cube-projected procedural sky
  (each texel coloured from its view direction), so shared edges match by
  construction; horizon fades to engine fog colour. sky.x now has 6 faces
  with texel-centre inset UVs. Regression: tools/tests/test_sky_atlas.py
  (checks the quantized BMP).
- Browser verified (OPENXOPS_TEST_HOOKS build, new qa_pose_preview /
  qa_sky_preview hooks): four-view soldier close-ups read as clothed soldiers
  holding the pistol; sky-only renders at horizon, cube corner and zenith show
  no seams, clouds continuous. 159 tool tests pass. Commits d47283d, b1b91e1.
- Delegation switched to anthropic claude-sonnet-5 (low reasoning). Workers:
  vp-meta (provenance/manifest refresh, sky2-5, docs) and vp-guard (prove
  _qa_* exports never reach production; add stage guard + test).
Remaining before deploy: merge workers, full no-ZIP starter pass (move, fire,
enemies visible), classic ZIP overlay smoke, PR to main, CI green, deploy.

Worker results merged (Sonnet): vp-guard 991cd4e (stage_pages rejects any
`_qa_` export; previously only checked a stale symbol name), vp-meta 5c46ac8
(28 provenance hashes refreshed, procedural sky1-5 records, sky2-5 generated,
cloud mask widened to remove a corner sub-texel artefact on skies 2/5,
manifest sky slots generated, docs). Parent found and fixed a gameplay defect
during QA: the starter's open south gate let players back off the floor edge
and fall to death; gate closed + perimeter regression (4652fea).
Release acceptance on a clean production build (no QA exports, 0 `_qa_`
symbols): opening shows the procedural sky; no-ZIP STARTER briefing -> play,
strafing and firing work (ammo 15->14), enemies render as uniformed soldiers;
classic ZIP overlay loads the original opening. 161 tests pass, 0 provenance
failures (32 pre-existing missing-P0-slot gaps unchanged). PR to main next.

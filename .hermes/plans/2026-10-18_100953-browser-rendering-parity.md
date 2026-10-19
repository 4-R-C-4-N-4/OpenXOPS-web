# Browser Rendering-Parity Cleanup Implementation Plan

> **For Hermes:** Use subagent-driven-development to implement this plan task-by-task. Use gpt-6-luna with low reasoning for bounded audits, tests and mechanical fixes; cap concurrency at three. The parent owns runtime reproduction, root-cause decisions, integration and live verification. Do not accept source-only review as proof of rendered output.

**Goal:** Make scoped reticles, weapon displays and original game assets render reliably in the browser preview, without changing intentional original gameplay or regressing the working live build.

**Architecture:** Repair the currently shipped legacy-GL compatibility backend first. Diagnose each symptom across archive presence → VFS lookup → decoder/resource handle → draw submission → actual pixels. Keep the experimental GLES3 rewrite and CC0 replacement pack out of this bug-fix release.

**Tech stack:** C++17 game logic, SDL2, Emscripten 6.0.12 legacy GL, WebGL, browser ZIP/VFS, IndexedDB; Python unit tests; proposed dev-only Playwright visual/integration tests.

---

## 1. Grounded starting point

Repo: `/home/ivy/Work/OpenXOPS-web`.
Live preview: `https://4-r-c-4-n-4.github.io/OpenXOPS-web/`.
Inspected baseline: `d3ebefd` (read-only investigation; working tree clean).
Local reference archive: `/home/ivy/Downloads/xops096.zip` (never commit/publish it).

### Confirmed source facts, not runtime diagnoses

- PSG1 is `Weapon[2]`, uses `data/model/weapon/psg1.x` / `psg1.bmp`, and `scopemode=2`: `src/parameter.cpp:571–604`.
- Scope mode 2 has `ScopeGunsight=2` and `NoScopeGunsight=false`: `src/parameter.cpp:1850–1853`. Thus no ordinary unscoped PSG1 crosshair is intentional; scoped PSG1 should get the long black crosshair drawn by `src/gamemain.cpp:2957–2970`.
- Scope texture and reticle geometry are distinct passes: `src/gamemain.cpp:2935–2970`. Inspect both; replacing `scope.dds` cannot fix omitted line draws.
- The ordinary aiming marker is conditionally drawn at `src/gamemain.cpp:3167–3175`. Do not globally force a crosshair on every weapon.
- Arms display defaults off in `src/config.cpp:657–663`, initializes `Camera_HOMEmode` at `src/gamemain.cpp:1764`, and toggles on Home at `:2457–2465`. `ObjectManager::Render` selects hide-player / draw-arms-only at `src/objectmanager.cpp:3057–3070`. First-person held guns and the spinning HUD inventory gun models are different paths.
- HUD inventory guns use `SetWorldTransformPlayerWeapon`, `gluUnProject`, and `glTranslated`: `src/platform/portable/d3dgraphics-portable.cpp:1667–1715`; draw calls at `src/gamemain.cpp:3257–3278`. This path deserves special scrutiny because the port already needed compatibility replacements for other GLU/matrix calls. This is a hypothesis, not a confirmed GLU bug.
- The active renderer silently returns on missing model handle at `src/platform/portable/d3dgraphics-portable.cpp:1973–1974`, and falls back to untextured geometry for failed textures at `:1985–2000`. Asset absence and failed rendering currently look similar to the user.
- Its 2D helpers manipulate projection/modelview/depth/culling, but not all GL state: `Start2DRender :2274`, `End2DRender :2779`; scope reticle lines use `Draw2DLine :2552`. Fog, alpha, stale texture coordinates, clipping and emulation state are candidates to test, not assumptions to patch wholesale.
- Read-only archive inventory found all nonempty weapon model/texture references present. There are 23 weapon definitions, including intentionally empty `BugWeapon` index 0. The 22 other definitions' referenced files exist; this does NOT prove decoding/drawing succeeds.
- Of those referenced model headers, 19 are `xof 0302txt 0064` and three are `xof 0302txt 0032Header {`. Include both exporter variants in decoder tests; the active parser checks only the initial `xof 0302txt` prefix.

### Scope and constraints

In scope: missing scoped reticles, invisible held/HUD weapons, unexpected missing original textures/models/props/effects/briefing assets, and rendering-state contamination causing those symptoms.

Not in scope: new art, graphics modernization, multiplayer, broad code cleanup, translating Japanese text, or changing weapon balance. Existing Japanese `?` placeholders remain separately tracked unless they directly prevent navigating reproduction cases.

Keep production live while fixes are developed locally. Never store copyrighted reference screenshots or original archives in Git/Pages artifacts.

## 2. First milestone: a reproducible symptom matrix

**Estimate:** half a focused day. Start here, before changing renderer behaviour.

### Task A1 — deterministic repro scenarios

**Create:** `docs/RENDERING-BUGS.md`, `tools/browser-qa/package.json`, `tools/browser-qa/playwright.config.*`, `tools/browser-qa/tests/weapons.spec.*`.

1. Reproduce the user's PSG1 observation and record whether scoped/unscoped; record exact mission, inventory, camera and Home state.
2. Capture separate scenarios for held first-person weapons, third-person/dropped weapons, and HUD inventory models.
3. Define a local deterministic test mission or debug-only fixture that cycles every normal weapon without walking around to collect them. Store authored minimal fixture/selection metadata, not original map/model files.
4. Add dev-only browser automation with explicit local archive input (`XOPS_TEST_ARCHIVE`). Choose test runner settings that keep the tab foregrounded and wait for engine ticks, not arbitrary short click delays.
5. Missing archive must report a clear skipped integration test, never a fabricated passing visual test. CI synthetic tests still run without original data.
6. Record original behaviour from the upstream source plus a native Windows/reference run if a compatible runtime is available. If not available, label screenshot parity unavailable; do not assume the legacy GL backend is the sole correct reference over D3D.

**Gate:** a repeatable browser test reaches PSG1 scoped and unscoped, at least one known-visible and one reported-invisible gun, Home off/on, and the two HUD weapon slots. Screenshots/traces are saved only under ignored `build-*` or scratch.

### Task A2 — classify the reported symptoms

For each case record: expected original behaviour, observed browser pixels, archive path, VFS resolution, load result, draw path, and reproducibility. Treat 'no unscoped PSG1 sight' and 'Home off hides held gun' as intentional until the user requests changed defaults.

**Gate:** the issue ledger distinguishes confirmed defects from intentional defaults; no blanket 'all models missing' or 'fix all crosshairs' ticket.

## 3. Scoped reticle repair (first user-visible fix)

**Estimate:** half to one focused day, depending on emulation state.

**Likely modifications:**
- `src/platform/portable/d3dgraphics-portable.cpp` (`Start2DRender`, `Draw2DLine`, `Draw2DBox`, `End2DRender`).
- `src/gamemain.cpp` only if the draw request itself is wrong; preserve scope tables.
- New synthetic reticle tests in `tools/browser-qa/tests/reticles.spec.*`.

### Task B1 — prove where the scoped reticle disappears

1. Write a failing test for a synthetic scope overlay and contrasting background, using the same production 2D line/box helpers.
2. Assert the expected mode-2 centre-row/column coverage, rather than visual difference anywhere in the frame.
3. Trace whether `ScopeGunsight=2` is selected and whether line calls reach the compatibility renderer.
4. Sample GL state before/after overlay: matrices, viewport, fog, blend, alpha test, texture enabled/bound, depth and cull. Do not leave per-frame console spam enabled by default.

### Task B2 — minimal isolated fix

Fix only the confirmed failed state or emulation path. Prefer explicit overlay state and deliberate restoration; if native WebGL line width/legacy lines are unreliable, use pixel-width quad geometry for the reticle with unchanged positions/colours. Do not alter all gameplay reticles by assumption.

**Gate:** PSG1 scoped reticle visible at logical centre; intended no unscoped PSG1 marker preserved; other scope modes still correct; reticle survives fullscreen, DPR changes and repeated HUD/world passes. Expected results must be measured in game pixels, not CSS pixels.

## 4. Weapon visibility and transforms

**Estimate:** one focused day for common-path fixes; expand only after classification.

**Likely modifications:**
- `src/platform/portable/d3dgraphics-portable.cpp`: `LoadModel`, transforms, `RenderModel`, GLU call adapters.
- `src/resource.cpp:330–350` for resource diagnostics, not weapon asset replacements.
- `src/object.cpp:2125+`, `src/objectmanager.cpp:3029+`, `src/gamemain.cpp:3257+` only where tracing proves caller errors.
- `src/config.cpp` / web controls only for a separately labelled preference/UI change.

### Task C1 — distinguish held guns from HUD icons

Repro with Home enabled, both inventory slots, reload, switching, scoped state, and first-/third-person camera. Confirm whether intentional body/weapon hiding is being mistaken for a decoder fault. Document Home in visible controls. If default-on arms are desired, make that an explicit saved browser preference after parity—not a silent global default change.

### Task C2 — resource/geometry proof

1. Write tests loading synthetic `.x` fixtures for triangles, quads, multiple meshes, UVs and both observed header variants.
2. In the local original-data browser run, enumerate the 22 normal weapon definitions, record model and texture decode results and nonzero geometry counts, and verify vertices/UVs/indices finite and in range.
3. Draw one failing weapon with a controlled identity camera and known transform. Distinguish parse failure, winding/culling, alpha/texture failure, camera/frustum placement and HUD-only unprojection failure.

### Task C3 — transforms and render-state fix

Compare `gluUnProject`/`glTranslated` behaviour to a tested local matrix implementation before replacing anything. Test round-trip project/unproject, camera translation/rotation and near-plane placement. If GLU is the failure, replace only that call path and add numeric regression tests. Confirm world weapon models, arm attachment and inventory models independently.

**Gate:** every supported nonempty weapon definition loads and is demonstrably rendered in its intended display mode; no forced gun model while scoped if upstream intentionally hides it; main/sub HUD models visible and correctly placed; Home toggles only expected player geometry. Remaining exceptions must be explicitly explained, not silently marked green.

## 5. Asset coverage and silent-failure elimination

**Estimate:** half to one focused day, then fix confirmed decoder categories.

**Create:** `tools/audit_original_assets.py`, `tests/fixtures/assets/` (synthetic only), browser QA asset-report tests.
**Modify as necessary:** `src/resource.cpp`, `src/platform/vfs.cpp`, active renderer loaders and `web/index.html` diagnostics.

### Task D1 — read/resolve/decode audit

Build a machine-readable local report from source references, dynamic mission/map paths, and the selected ZIP. Categories:
- present and decoded;
- intentionally absent/unused;
- optional addon absent;
- required absent;
- VFS resolution failed;
- unsupported/corrupt format;
- resource capacity exhausted;
- valid loaded resource, but not submitted/drawn.

Cover weapon/human/prop models, skins, map tiles, skies, `char.dds`, `title.dds`, scope and effects, briefing art. Do not call file existence alone a rendering pass. Preserve original archive bytes and expected path/CP932 semantics.

### Task D2 — diagnosable loader failures

Add opt-in structured diagnostics with asset path, resolved layer, decoder, failure reason and resource id. Deduplicate repeated failures; keep release playback free of frame-by-frame logs. Expose a user-copyable diagnostics summary without including the ZIP, private file paths or image bytes.

### Task D3 — fix the proven format/state gaps

Add failing synthetic tests first for actual unsupported BMP/DDS/.x categories. Validate lengths/dimensions/indices before allocations or reads; preserve alpha channels and mask semantics intentionally. Make missing required assets actionable, not an untextured/empty screen with no explanation.

**Gate:** all expected boot + test-mission resources accounted for, all normal weapon assets decoded, no unexplained required-resource failures, and browser screenshots show the previously missing items. Do not substitute our incomplete CC0 pack to disguise original-data bugs.

## 6. Regression gates and release

**Estimate:** half a focused day after fixes; first reticle patch may ship earlier once independently verified.

1. Run existing tests:
   ```sh
   python3 -m unittest discover -s tools/tests -v
   python3 -m unittest discover -s tools/artgen -v
   python3 -m unittest discover -s tools/sfxgen -v
   cmake --build build-audio-integrated -j4
   ctest --test-dir build-audio-integrated --output-on-failure
   ```
2. Rebuild the actual shipped target:
   ```sh
   bash tools/build-playable-web.sh
   python3 tools/stage_pages.py
   ```
3. Run the proposed dev-only browser suite (after its package exists):
   ```sh
   npm ci --prefix tools/browser-qa
   XOPS_TEST_ARCHIVE="$HOME/Downloads/xops096.zip" \
     npm test --prefix tools/browser-qa
   ```
   The suite starts/stops its own localhost server, uses the packaged build, and does not require private archives in public CI.
4. Browser matrix: Chromium first, Firefox second. Verify crosshairs, weapon display matrix, representative props/effects, cached ZIP start, invalid ZIP rejection, forget cache, fullscreen and screenshot-reproduced monitor-sized backing-buffer resize.
5. Compare internal 640×480 pixel crops, not fullscreen screenshot dimensions. Do not publish original asset reference screenshots with the repo or Pages build.
6. Independent spec/correctness review by Luna, parent checks against actual upstream semantics, and parent runs browser tests. Reject review suggestions that 'fix' intentional centred audio/no-scope/arms defaults without reading the source.
7. Commit narrow fixes, update `docs/RENDERING-BUGS.md`, `docs/RUNNING.md` and `PROGRESS.md`, let existing Pages workflow deploy, then verify the public URL with the actual original archive. Read back deployed fingerprint and test the exact repaired gameplay states.
8. Keep previous known-good commit available for rollback; revert a failing patch rather than editing shipped binaries manually.

## 7. Delegation and effort controls

- First parallel tasks: scoped-reticle investigation; weapon display/load inventory; archive/asset coverage audit. Read-only until each has a runtime reproduction or a concrete failing synthetic test.
- Parent chooses the shared renderer/state root cause. Only one implementation worker owns `d3dgraphics-portable.cpp` at a time, so concurrent fixes do not fight over GL state.
- Keep worktrees separate, cap three low-reasoning Luna workers, and give each exact test gates. Do not repeat broad 'finish graphics' requests after incomplete reports.
- If a bounded worker cannot explain its failing test, parent inspects it before another retry. Escalate one hard diagnosis if necessary, rather than spending many low-cost attempts without progress.
- The native experimental GLES driver leak does not gate these browser-renderer fixes. App-owned memory errors and actual browser failures do gate them.

**Budget:** roughly 2–4 focused engineering days for diagnosis, common-path fixes and release verification if these are shared compatibility-state/transform bugs. Wider decoder gaps may add work; reassess after the first reproducible inventory rather than promising all assets fixed from file presence alone.

## 8. Acceptance criteria

- Scoped PSG1 has its expected reticle; intentionally absent unscoped reticle remains unchanged unless explicitly requested.
- User can see intended first-person held weapons with Home/arms enabled; inventory HUD weapon models and world models also render correctly.
- Each normal weapon definition is tested, including weapons sharing models/skins and the observed `.x` exporter variants.
- Missing-asset reports distinguish absent from decode/render failures; no unexplained required asset failures in chosen reference missions.
- Cache, window scaling/fullscreen, original-data privacy and automatic deployment remain working.
- Live public build reproduces the local fixes; no success claim based only on compilation or the triangle harness.

## Open questions (non-blocking for investigation)

1. Was the reported PSG1 sight missing while scoped, unscoped, or both? Test both regardless; scoped is a bug candidate and unscoped absence is intentional.
2. Did 'first-person guns' mean held arms/gun, the rotating HUD gun thumbnails, or both? Test both rather than making the user diagnose it.
3. Should arms be enabled by default in the web version? Recommend preserve current upstream default during fixes, expose/document Home, then choose a preference separately.
4. Which additional missing assets did the user notice? Asset audit proceeds without waiting, and can absorb screenshots/mission names later.

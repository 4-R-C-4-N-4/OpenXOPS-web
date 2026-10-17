# OpenXOPS-web — Scope of Work: Browser-Playable Build

**Author**: IV (with Claude Code)
**Project**: OpenXOPS-web (fork of OpenXOPS 1.080)
**Document type**: Design / implementation scope
**Status**: Draft
**Version**: 0.1
**Date**: 2026-10-17
**Depends on**: OpenXOPS 1.080 source (`src/`, tag `upstream-1.080`); openxops.net redistribution policy (`developer-original.php`); XOPS 0.96 file list (`filesystem-xops096filelist.php`)

---

## 1. Scope

### 1.1 Goal

Ship a single-player OpenXOPS that runs in a desktop web browser from a URL
(GitHub Pages), using the player's own copy of X operations 0.96 data. "Online
player" in this document means **playable at a URL with no install**. It does
not mean networked multiplayer (see §11, Future Work).

### 1.2 In scope

- Replacing the four Win32/DirectX platform classes (`WindowControl`,
  `InputControl`, `SoundControl`, `D3DGraphics`) with portable SDL2 / OpenAL /
  OpenGL ES 3 (WebGL 2) implementations.
- A native Linux build of the same backends, used as the debug target.
- An Emscripten (wasm) build target.
- A web shell: data onboarding (user supplies `xops096.zip`), persistence,
  pointer lock, audio unlock, license display.
- CI that builds wasm and deploys it to GitHub Pages.

### 1.3 Out of scope

- **Bundling X operations graphics or sound data.** This is forbidden by the
  upstream redistribution policy (§3.1). The deployed site must contain zero
  bytes of XOPS data.
- Using `ezds.dll` or `config4xops05.exe` (nine-two's files). Neither is needed.
- Networked multiplayer (§11).
- Mobile and touch input, and gamepads. These are deferred (OQ-6).
- Gameplay changes. Game logic stays bit-for-bit upstream behavior, except
  where a platform seam forces a change.
- Removing the DirectX backend. Upstream files stay in place but are excluded
  from our build.

## 2. Terminology

| Term | Definition |
|---|---|
| XOPS / 本家XOPS | X operations 0.96, the original 2003–2006 freeware FPS by Team MITEI. |
| XOPS data | Contents of `xops096.zip`: `data/`, `addon/`, mission `.mif`/`.bd1`/`.pd1`, `.x` models, `.bmp`/`.dds` textures, `.wav` sounds. Copyrighted by TENKUU and others. Not redistributable. |
| `xops096.zip` | The canonical XOPS 0.96 archive. Size 5,015,575 bytes, MD5 `020d4aa49c2501b06c6ea3e9c2df454d`. |
| Upstream | OpenXOPS 1.080 by [-_-;](mikan), New BSD. Mirrored verbatim at tag `upstream-1.080`. |
| Platform class | One of `WindowControl` (window.h), `InputControl` (input.h), `SoundControl` (sound.h), `D3DGraphics` (d3dgraphics.h). Together they are the only code that touches Win32/DirectX/WGL. |
| Game logic | Every other `src/*.cpp` file. Verified to contain no Win32 types or calls (grep for DWORD/HWND/WINAPI/timeGetTime/MessageBox/HDC/HRESULT → 0 hits in 14 files). |
| Backend | Our new implementation of a platform class, selected by the existing `GRAPHIC_ENGINE` / `SOUND_ENGINE` / `INPUT_INTERFACE` macros, plus a new `WINDOW_BACKEND` macro. |
| VFS | Virtual file system layer (`vfs.h`/`vfs.cpp`, new) that all game-data reads go through. |
| CP932 | Microsoft's Shift-JIS variant. The encoding of the upstream source and of all XOPS text data. |
| Tick | One call of `ProcessScreen()`. Fixed at `GAMEFRAMEMS` = 30 ms (33.3 Hz). |

## 3. Constraints

### 3.1 Licensing (normative)

These rules come from openxops.net `developer-original.php`, retrieved 2026-10-17.

- C-1: The OpenXOPS BSD copyright notice, license text and disclaimer MUST
  appear in both the source and the documentation. The deployed web build MUST
  show them on a reachable page. The upstream-sanctioned in-game option
  (`OPTIONS → SOFTWARE LICENSE`) is also acceptable.
- C-2: XOPS graphics data MUST NOT be distributed with the build. The player
  supplies it themselves.
- C-3: `ezds.dll` and `config4xops05.exe` MUST NOT be shipped or used.
- C-4: The "OpenXOPS" name MUST NOT be used to imply endorsement (BSD clause 3).
  The README states that this is an unofficial fork.

### 3.2 Technical

- T-1: GitHub Pages cannot set COOP/COEP headers, so `SharedArrayBuffer` is
  unavailable. **The build MUST NOT use `-pthread`.** Upstream is
  single-threaded, so this costs nothing.
- T-2: Browsers require a user gesture before audio playback and pointer lock.
  The shell needs a "Click to play" gate.
- T-3: In browsers, Esc always exits pointer lock and cannot be intercepted.
  Upstream uses Esc (`GetEscKeycode()`, 7 call sites in gamemain.cpp) for
  menu-back and quit (see §8.3).
- T-4: The tick rate is 33.3 Hz and both update and render happen inside
  `ProcessScreen()`. The port preserves this. Render interpolation is out of
  scope (OQ-4).

## 4. Current architecture (grounded)

```
main.cpp  WinMain → InitGame → for(;;){ CheckMainLoop(); ProcessScreen(); ControlFps(); }   (main.cpp:265)
            │
            ├─ game logic (25k LOC, portable): gamemain, objectmanager, object, ai,
            │   collision, event, parameter, resource, datafile, config, scene, gui-object, statemachine
            │
            └─ platform classes (Win32-bound):
                 WindowControl  window.cpp   408 LOC  PeekMessage/WaitMessage, timeGetTime, Sleep, MessageBox
                 InputControl   input.cpp    855 LOC  GetKeyboardState, Raw Input, Get/SetCursorPos, VK codes
                 SoundControl   sound-ezds.cpp / sound-directsound.cpp   ezds.dll or DirectSound 3D
                 D3DGraphics    d3dgraphics-directx.cpp (D3D9)  |  d3dgraphics-opengl.cpp (GL 1.1 + WGL, GRAPHIC_ENGINE 8)
```

Facts that shape the plan:

- F-1: `D3DGraphics` has about 55 public methods (d3dgraphics.h:303–366).
  `SoundControl` has 9, `InputControl` 17, `WindowControl` 8. Game logic
  depends only on these interfaces.
- F-2: The GL backend is fixed-function GL 1.1. It uses client-state arrays
  (`glVertexPointer`/`glDrawArrays`), the matrix stack, `glFog`,
  `glAlphaFunc` and display lists, with text drawn by
  `wglUseFontBitmapsW` + `glCallList`. WebGL supports none of these.
- F-3: The default `SOUND_ENGINE 0` (ezds) computes volume and pan in game code
  (`CheckSourceDist`, `CalculationVolume`). A backend that plays a buffer at a
  given volume and pan therefore reproduces original audio behavior exactly,
  with no positional-audio API needed.
- F-4: Input is already abstracted. Game logic calls
  `CheckKeyNow/Down/Up(OriginalkeycodeToDinputdef(xopsCode))`, and the id
  space is Windows VK codes. A backend only has to fill a `keys[256]` VK
  array.
- F-5: All file I/O is `fopen` (about 20 sites in datafile, config, the GL
  backend and sound) plus one `FindFirstFile` (datafile.cpp:1267, the addon
  `.mif` scan). Paths use `\\`. Upstream already has
  `ENABLE_PATH_DELIMITER_SLASH` + `ChangePathDelimiter()` (main.h:79) at most
  read sites. The GL backend's loaders lack the call.
- F-6: Texture formats are BMP, DDS (custom loader, including A4R4G4B4 for
  `char.dds`), and JPEG/PNG behind `ENABLE_OPENGL_LIBJPEGPNG`. Models are
  text `.x` with a custom parser.
- F-7: Source text is CP932 + CRLF. Japanese string literals exist, but only
  in `ErrorInfo()` message boxes and debug output, not in-game text. In-game
  Japanese (briefings, messages) comes from XOPS data in CP932, so the
  text renderer must decode CP932 at runtime.
- F-8: The upstream author released a DxLib port sample of 1.080 (2026-08-12,
  `sample_1.080_DxLib3.24f.zip`) that replaces render, input and sound. It is
  a reference for where the upstream author cuts the seams.

## 5. Target architecture

```
src/                     upstream game logic (minimally patched)
src/platform/
  window-sdl.cpp         WindowControl     (WINDOW_BACKEND_SDL)
  input-sdl.cpp          InputControl      (INPUT_INTERFACE 3)
  sound-openal.cpp       SoundControl      (SOUND_ENGINE 3)
  d3dgraphics-gles.cpp   D3DGraphics       (GRAPHIC_ENGINE 9)  — GLES 3.0 / WebGL 2
  gles-matrix.h          4×4 matrix stack replacing glMatrixMode/glRotatef/…
  textrender.cpp         CP932 → Unicode decode + glyph atlas
  cp932_table.h          generated
  vfs.cpp / vfs.h        game-data reads (zip or directory) + writable save dir
  mainloop.cpp           main() / emscripten_set_main_loop + fixed-tick accumulator
web/
  shell.html, loader.js  data onboarding, IndexedDB, click-to-play gate, license page
CMakeLists.txt           targets: openxops (native Linux), openxops-web (emcc)
```

All backends compile on both targets. The Emscripten build differs only in
flags and in `mainloop.cpp`/`vfs.cpp` ifdefs. Native Linux exists so we can
debug with gdb and AddressSanitizer at native speed.

## 6. Work breakdown

Effort is focused engineering days. ★ = acceptance gate for the phase.

### Phase 0 — Repo and toolchain (0.5–1 d)

- P0.1 Convert `src/` to UTF-8 + LF in **one mechanical commit**, using
  `iconv -f CP932`. **Do not use `-f SHIFT_JIS`**: it maps byte 0x5C to `¥`
  (U+00A5) and breaks every `"data\\model\\..."` escape. Verify with
  `git diff --stat` that only bytes inside comments and literals changed, and
  that a round trip back to CP932 is byte-identical.
- P0.2 Add `CMakeLists.txt` with native target only, game logic plus stub
  platform classes, to prove the 25k LOC compiles under clang/gcc. Expect
  MSVC-isms to fix: `_stricmp`, `<direct.h>` (main.h:68), `#pragma comment`,
  `_CRTDBG`.
- P0.3 Pin emsdk (latest 4.x) and document `source emsdk_env.sh`.
- P0.4 `.gitignore` the `xopsdata/` directory. The developer places
  `xops096.zip` there. `tools/check-data.sh` verifies the MD5.
- P0.5 Download the DxLib sample and record the seam differences in
  `docs/notes/dxlib-sample.md`.
- ★ `cmake --build build-native` links (with stubbed platform) and CI runs it.

### Phase 1 — Native Linux port (3–5 d)

- P1.1 **VFS** (`vfs_fopen`, `vfs_list`). Replace the about 20 `fopen` reads
  and the one `FindFirstFile`. Specified in §7.
- P1.2 **WindowControl/SDL**: `SDL_CreateWindow` + GLES 3.0 context.
  `CheckMainLoop()` pumps `SDL_PollEvent` and returns -1/0/1 with upstream
  semantics. `ErrorInfo` uses stderr + `SDL_ShowSimpleMessageBox`.
  `GetTimeMS` uses `SDL_GetTicks`.
- P1.3 **InputControl/SDL**: SDL scancode → VK table (static 512-entry array).
  Mouse via `SDL_SetRelativeMouseMode`. `MoveMouseCenter()` becomes a no-op in
  relative mode. `GetMouseMovement` returns accumulated `xrel/yrel`.
- P1.4 **SoundControl/OpenAL**: reuse the upstream WAV header parser
  (`CheckSoundFile`, sound-directsound.cpp) and keep the `SOUND_ENGINE 0`
  game-side volume/pan math (F-3). Hold up to `MAX_LOADSOUND` (24) buffers and
  a shared pool of `MAX_SOUNDLISTS` (100) sources. Map pan to `AL_POSITION`
  on a unit circle with `AL_SOURCE_RELATIVE`. Map volume to `AL_GAIN` using
  the dB curve of `GetDSVolume` (sound-directsound.cpp).
- P1.5 **D3DGraphics/GLES** (the largest task, about 2–3 d). Port
  `d3dgraphics-opengl.cpp` method by method:
  - One shader pair. Uniforms: `u_mvp`, `u_useTexture`, `u_alphaRef`
    (replaces `glAlphaFunc`), `u_fogColor`/`u_fogStart`/`u_fogEnd` (linear,
    matching `SetFog` sky tables), and a vertex colour attribute.
  - `gles-matrix.h` replaces the `glMatrixMode`/`glLoadIdentity`/
    `glRotatef`/`glTranslatef`/`glScalef`/`glPushMatrix` calls 1:1.
  - Map geometry (`LoadMapdata`) goes into one static VBO per texture batch.
    Models go into a VBO each. Dynamic 2D/effect quads use a streaming VBO.
  - Texture upload converts BMP 8/24-bit, DDS A8R8G8B8/A4R4G4B4, and optional
    PNG/JPEG via Emscripten ports to RGBA8. `BlackTransparent` handling stays
    as upstream.
  - `SaveScreenShot` uses `glReadPixels` → BMP written through VFS.
- P1.6 **Text** (`textrender.cpp`). `Draw2DMSFont*` and `GetFontTextSize`
  decode CP932 → UTF-32 (generated table), then draw from a glyph atlas built
  on demand from the bundled font (OQ-1). `Draw2DTextureFont*` (the
  `char.dds` font) ports unchanged.
- P1.7 **Main loop** (`mainloop.cpp`). Extract `MainLoopStep()`. Replace
  `ControlFps()`/`Sleep` with an accumulator (§8.1).
- P1.8 Enable `ENABLE_AUTOCREATECONFIG` (main.h:78), so `config.dat` is
  generated when absent.
- ★ The native binary, pointed at an extracted `xops096.zip`, plays opening →
  menu → briefing (Japanese text rendered) → a full mission → result, with
  sound and mouse look, under ASan with no errors.

### Phase 2 — Emscripten target (1–2 d)

- P2.1 Add an `openxops-web` CMake target with flags from §9.
- P2.2 `emscripten_set_main_loop(MainLoopStep, 0, 1)`. Rendering waits for
  rAF; the accumulator runs 0..N ticks per rAF.
- P2.3 Pointer lock: request on canvas click and re-request after a release.
  Implement the Esc remap from §8.3.
- P2.4 Audio unlock: create the OpenAL context on first gesture and resume
  the `AudioContext` on `visibilitychange`.
- P2.5 Canvas sizing: 4:3 logical resolution from config (default 640×480),
  CSS-scaled letterbox, `devicePixelRatio`-aware backing store.
- P2.6 `WindowControl::CloseWindow()` (quit from the menu) leaves the main loop
  and shows a "Session ended — Play again" overlay.
- ★ Chrome and Firefox desktop: a full mission is playable, input feels
  identical to native, and there are no console errors.

### Phase 3 — Web shell and data onboarding (1–2 d)

- P3.1 First-run screen: "OpenXOPS needs X operations 0.96 data", with a link
  to the Vector download page and a drop zone / file picker for
  `xops096.zip`.
- P3.2 Validation: MD5 mismatch warns but does not block, because repacked
  archives exist. Required entries missing (`data/char.dds`,
  `data/map10/temp.bd1`) blocks with a list of what is missing.
- P3.3 Persistence: the raw zip bytes are stored in IndexedDB under
  `IDB_DATA_KEY`. Later visits boot straight in. A "Forget data" button clears
  them.
- P3.4 Optional addon zips are stored as additional IndexedDB entries and
  mounted under `addon/`.
- P3.5 The writable save dir (`config.dat`, screenshots, logs) uses
  IDBFS at `/save`, synced after writes.
- P3.6 License page (C-1) and an "unofficial fork" notice (C-4).
- ★ A fresh browser profile can go from site URL to playing in under 1 minute,
  and a reload resumes without re-upload.

### Phase 4 — CI and deploy (0.5 d)

- P4.1 GitHub Actions: `setup-emsdk` pinned → build → upload Pages artifact.
  A native build runs as a compile check.
- P4.2 Guard: CI fails if any file in the Pages artifact matches an MD5 from
  the XOPS 0.96 file list (C-2).
- ★ The `main` branch auto-deploys to `https://4-r-c-4-n-4.github.io/OpenXOPS-web/`.

### Phase 5 — QA and polish (1–3 d)

- P5.1 Render parity checklist against Windows OpenXOPS 1.080 screenshots
  (fog, alpha-tested foliage, muzzle flash blend, scope overlay, HUD, briefing
  images).
- P5.2 Browser matrix: Chrome, Firefox, Safari 17+ (WebGL 2).
- P5.3 Performance: target a steady 33.3 Hz tick on a 2015-era laptop iGPU.
  Frame budget logged in debug builds.
- P5.4 Optional web config UI (replacing `config4xops05.exe`): key bindings,
  sensitivity, volume, written to `config.dat` (OQ-5).

**Total: about 7–13 focused days.**

## 7. VFS specification

```
// Read path: game data (read-only)
FILE* vfs_fopen(const char* path, const char* mode)
  if mode contains 'w' or 'a': return save_fopen(path, mode)        // §7.2
  key = vfs_normalize(path)
  entry = index.find(key)
  if !entry: return NULL                                            // callers already handle NULL
  return fmemopen(entry.data, entry.size, mode_without_t(mode))     // glibc + Emscripten musl both provide fmemopen

vfs_normalize(p):
  strip leading "./" and ".\\"
  replace every '\\' with '/'         // byte 0x5C. Note: CP932 trail bytes can be 0x5C — see below
  ASCII-lowercase bytes 'A'..'Z' only (non-ASCII bytes untouched)
  collapse "//" → "/"

int vfs_list(const char* dir, const char* ext, char out[][MAX_PATH], int max)   // replaces FindFirstFile
```

- CP932 trail-byte hazard: the second byte of a double-byte character can be
  0x5C (`表` = 0x95 0x5C, `ソ` = 0x83 0x5C, verified with iconv).
  `vfs_normalize` MUST walk the string as CP932 (lead bytes 0x81–0x9F and
  0xE0–0xFC consume the next byte) and only rewrite a 0x5C that stands alone.
  **Upstream `ChangePathDelimiter` (window.cpp:393) is not DBCS-aware: it
  rewrites every 0x5C byte.** This is one reason v1 leaves
  `ENABLE_PATH_DELIMITER_SLASH` off.
- Index keys are raw normalized **bytes**, not UTF-8. Zip entry names in
  `xops096.zip` are CP932 without the UTF-8 flag (for example the texture
  referenced by `data/article/isu.x`). Keying on bytes makes them match the
  CP932 strings the game passes, with no transcoding.
- Sources: on native, a directory tree scanned at boot. On web, zip archives
  parsed in C++ (central directory + `inflate` from the zlib we already link).
  The Emscripten FS is not used for game data.
- Errors: a duplicate key across archives is resolved last-mounted-wins
  (addons override base). A corrupt zip makes `vfs_mount_zip` return
  `VFS_ERR_CORRUPT`, and the shell shows "archive is damaged" and does not
  boot.

### 7.2 Save path

`save_fopen` maps the normalized path under `SAVE_ROOT` (native:
`$XDG_DATA_HOME/openxops-web/`, web: `/save` on IDBFS). It handles
`config.dat`, screenshots (`ss*.bmp`) and the debug log. On web, every
`fclose` of a save file schedules `FS.syncfs(false)`, debounced by
`SAVE_SYNC_DEBOUNCE_MS`.

## 8. Behavioural details

### 8.1 Fixed tick

```
MainLoopStep():
  now = SDL_GetTicks64()
  acc += now - last; last = now
  if acc > MAX_CATCHUP_MS: acc = GAMEFRAMEMS          // tab was backgrounded; do not fast-forward the game
  while acc >= GAMEFRAMEMS:
      flag = MainWindow.CheckMainLoop()             // pumps SDL events
      if flag == -1: shutdown(); return
      ProcessScreen(..., framecnt++)                // includes render; last one wins the frame
      acc -= GAMEFRAMEMS
```

Each tick calls `ProcessScreen()` once, which includes rendering. Running
ticks back to back and presenting only the last one is acceptable, because
upstream draws a complete frame in every call. We do not separate update from
render (T-4).

### 8.2 Key mapping

`input-sdl.cpp` keeps `unsigned char keys[256]` indexed by VK code, so
`OriginalkeycodeToDinputdef` (input.cpp:582) is reused unchanged. The table
covers every VK that function can return. It also handles the special return
values -1 (mouse L), -2 (mouse R), -3 (either Shift) and -4 (either Ctrl).
Game logic handles these at gamemain.cpp:2009/2031, and
`InputControl::CheckInputAll` handles them at input.cpp:539–565. The new
backend MUST keep the `CheckInputAll` branch. A unit test asserts that
each XOPS original code 0x00–0x2E maps to a reachable SDL scancode.

### 8.3 Esc under pointer lock (web only)

The browser consumes Esc to release pointer lock (T-3). Behaviour:

```
on pointerlockchange → unlocked, while in maingame:
    inject one synthetic VK_ESCAPE KeyDown + KeyUp     // reproduces upstream "Esc = back to menu"
    show overlay "Click to resume"
on canvas click: requestPointerLock()
```

In menus (pointer not locked), Esc arrives as a normal keydown and passes
through. This decision is pending in OQ-3.

## 9. Configuration reference

| Key | Type | Default | Where | Description |
|---|---|---|---|---|
| `GAMEFRAMEMS` | int ms | 30 | main.h:52 (upstream) | Tick length. Not changed. |
| `DEFAULT_SCREEN_WIDTH/HEIGHT` | int | 640/480 | main.h:54 | Logical resolution. The canvas scales it. |
| `MAX_CATCHUP_MS` | int ms | 250 | mainloop.cpp | Max time debt before the accumulator resets. |
| `GRAPHIC_ENGINE` | int | 9 | d3dgraphics.h | 9 = new GLES 3 backend. |
| `SOUND_ENGINE` | int | 3 | sound.h | 3 = new OpenAL backend. |
| `INPUT_INTERFACE` | int | 3 | input.h | 3 = new SDL backend. |
| `ENABLE_AUTOCREATECONFIG` | define | on | main.h:78 | Generate `config.dat` if missing. |
| `ENABLE_PATH_DELIMITER_SLASH` | define | off | main.h:79 | Off: VFS normalizes instead (§7). |
| `ENABLE_DEBUGLOG` | define | native: on, web: off | main.h:75 | Writes through `save_fopen`. |
| `XOPS096_MD5` | string | `020d4aa49c2501b06c6ea3e9c2df454d` | loader.js | Soft validation. |
| `XOPS096_SIZE` | int | 5015575 | loader.js | Soft validation. |
| `IDB_NAME` / `IDB_DATA_KEY` | string | `openxops-web` / `xops096.zip` | loader.js | IndexedDB storage. |
| `SAVE_SYNC_DEBOUNCE_MS` | int ms | 500 | vfs.cpp | IDBFS sync debounce. |
| `FONT_ATLAS_SIZE` | int px | 1024 | textrender.cpp | Glyph atlas texture size, LRU-evicted. |
| emcc flags | — | `-O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 -sFULL_ES3=0 -lopenal -lidbfs.js -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sEXPORTED_RUNTIME_METHODS=FS` | CMakeLists.txt | No `-pthread` (T-1). |

## 10. Risks

| ID | Risk | Likelihood | Mitigation |
|---|---|---|---|
| R-1 | Render parity drift (fog curve, alpha ref, blend states) | High | P5.1 screenshot checklist; port GL state per method, never "approximately" |
| R-2 | Hidden MSVC-isms in game logic (UB that MSVC tolerates, `long` = 32-bit assumptions in file parsers) | Medium | Build native under ASan/UBSan in P1. Emscripten wasm32 has 32-bit `long` as MSVC does; native x86-64 Linux does not, so native will catch these first. |
| R-3 | CP932 0x5C trail byte in paths | Medium | §7 CP932-aware normalizer + unit test with `表`/`ソ` |
| R-4 | 33.3 Hz rendering feels choppy on 60–144 Hz displays | Medium | Accept for v1 (faithful to original). Interpolation is OQ-4. |
| R-5 | Safari WebGL 2 / audio quirks | Medium | Treat Safari as best-effort in v1 |
| R-6 | Some addon `.mif` files use paths/case the index misses | Low | Normalizer is case-insensitive. Log misses in debug builds. |

## 11. Future work: networked multiplayer (not in this SOW)

Recorded so the v1 design does not block it.

- Upstream is single-player, with AI, scripted events and one local player.
  Multiplayer needs: (a) a separate player/human ownership model in
  `objectmanager.cpp`/`gamemain.cpp` (about 8.5k LOC), (b) an authoritative
  server (a headless native build of the same game logic, which Phase 1 makes
  possible) speaking WebSocket, and (c) client-side prediction for the local
  player.
- Rough size: 3–6 weeks for basic 2–8 player deathmatch on existing maps.
- v1 decisions that keep this open: game logic stays free of platform calls;
  the VFS and main loop allow a headless tick (no `D3DGraphics` calls when
  `HEADLESS`).

## 12. Open questions

| ID | Question | Status |
|---|---|---|
| OQ-1 | Which font for Japanese text? Option A: Shinonome 12/16 px bitmap (public domain, JIS X 0208, about 300 KB). Close to the fixed-pitch "ＭＳ ゴシック" look; one code path. Option B: subset Noto Sans JP + stb_truetype (OFL, about 1–2 MB). Option C: Canvas2D `fillText` on web (0 bytes, but output varies by browser). Recommended: A. | open |
| OQ-2 | Should we tell the upstream author (mikan) about the fork? Not required, but invited by the redistribution page. Recommended: yes, once Phase 2 works. | open |
| OQ-3 | Is the Esc remap in §8.3 acceptable, or should we add an alternate back key (for example backtick)? | open |
| OQ-4 | Is render interpolation between ticks wanted post-v1? It requires splitting update and render in `ProcessScreen`. | deferred |
| OQ-5 | Should the web config UI (replacing `config4xops05.exe`) be part of v1? | open |
| OQ-6 | Is touch or gamepad support wanted? | deferred |
| OQ-7 | Should we track upstream SVN for post-1.080 changes, or treat 1.080 as frozen? | open |

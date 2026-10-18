# Play the browser preview

The OpenXOPS engine runs in Chromium with a bundled original free-data starter:
opening → STARTER → briefing → first-person gameplay. No archive is needed.
This is a prototype pack, not finished art or a replacement for all classic
content. An optional player-supplied original ZIP adds the classic missions.

## On this development machine

Open **http://127.0.0.1:8766/** while the preview server is running.

1. Leave the optional classic-data picker empty (use **Forget saved ZIP** if
   classic data was remembered from an earlier visit).
2. Click **Start game** to load the bundled free starter pack.
   Alternatively select your original `xops096.zip` before starting. It is read
   locally and remembered in IndexedDB, never uploaded to a server.
3. Press Enter to leave the opening scene.
4. Click **STARTER** (or **TRAINING YARD** if using classic data).
5. Left-click again on the briefing to begin.
6. The game switches to a centred play view that fills the available window
   while preserving 4:3. Use the **Fullscreen** button underneath for fullscreen.
   Click the canvas to capture the mouse. Esc releases it.

Classic (the original default configuration): W/S forward/back, A/D strafe,
arrow keys look, left mouse fires, Space jumps, Tab walks, Home toggles arm
visibility, R reloads, Left Shift aims/zooms the scope, X changes fire mode,
Z switches weapon, and 1/2 select weapon slots. A custom config may change
these defaults. The optional Modern preset uses Shift for walking and right mouse for scope,
left mouse to fire, Q to switch weapon, plus R, Space, X, G and 1/2 as shown
in its in-game guide. Presets apply next game start; the selected value applies
for the current page even if browser storage is unavailable, but will not
persist after reload.
The original archive is gitignored and is not included in builds or hosted
files. Obtain your own copy from the distribution linked in
[NOTICE.md](../NOTICE.md) if you do not have it.

## Tab/window lifecycle (pause and resume)

While a mission is running, the page pauses automatically if the tab is
hidden (switching tabs, minimizing) or the window/canvas loses focus
(Alt-Tab, clicking another app). A visible, keyboard-focusable **Resume
game** button appears over the canvas; play only continues once you click
it (or tab to it and press Enter/Space) while the tab is visible and
focused again. Simply switching back to the tab does not by itself resume
play if another pause reason (for example the Controls dialog) is still
open.

Multiple pause reasons (hidden tab, window blur, the Controls dialog) are
tracked independently: closing the Controls dialog while the window is
still unfocused keeps the game paused, since the focus-loss reason is
unresolved. On pause the page releases pointer lock and clears any
held/queued keyboard and mouse input (`Module._openxops_reset_input`), so
nothing keeps moving or firing once you resume.

Pressing Esc only to release pointer lock (without changing tab/window
focus) does not pause the game by itself.

## Build and serve

Requires Emscripten **6.0.12**, Python 3 and a desktop browser with WebGL.
On this machine the SDK is already installed at `~/programs/emsdk`.

```sh
cd ~/Work/OpenXOPS-web
source ~/programs/emsdk/emsdk_env.sh
bash tools/build-playable-web.sh
python3 -m http.server 8766 --bind 127.0.0.1 --directory build-web
```

Then open http://127.0.0.1:8766/ and select your archive. Stop the server with
Ctrl+C. If the port is occupied, reuse the running preview or select another
port and update the URL. Serve **only `build-web`**, not the repository root:
`xopsdata/` must never be exposed. Do not use a `file://` URL.

For a new SDK installation:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/programs/emsdk
cd ~/programs/emsdk
./emsdk install 6.0.12
./emsdk activate 6.0.12
source ./emsdk_env.sh
```

## What is actually verified

- Browser WASM build succeeds using `tools/build-playable-web.sh`.
- Chromium loads the selected original `xops096.zip` through the VFS.
- Original textured opening scene, animated menu characters and mission list
  render (not a substitute scene or triangle).
- Selecting Training Yard opens its briefing; left-click enters the mission.
- First-person Training Yard geometry/HUD render; keyboard look responds.
- The preview continues drawing at approximately 30 FPS on the test machine.

Audio and pointer lock have browser gesture restrictions. Audible mixing,
all weapons/missions, complete mission outcomes and other browsers have not
been exhaustively verified. Do not interpret successful boot as full parity.

## Known limitations

- Japanese GDI-font text is temporarily replaced by `?` characters; the
  original texture-font HUD/menu labels render. Some title/UI artwork has
  compatibility rendering defects.
- The preview uses Emscripten `LEGACY_GL_EMULATION` with the original GL
  renderer, adapted to SDL/VFS. This unblocks play; the independent GLES3
  shader rewrite remains experimental and is **not** required to boot it.
- The compatibility renderer converts client arrays to immediate-mode draws;
  it prioritizes working output rather than optimal performance.
- The validated archive is remembered in IndexedDB for this origin (same host
  and port). Private browsing, browser eviction or clearing site data can remove
  it. If storage fails, the game still plays but requires selection next time.
  Settings/save persistence is separate and not implemented yet.
- Quit ends the game loop; reload the page to start another session.
- Original game data is copyrighted and must not be deployed alongside the
  engine. The CC0 replacement pack is incomplete.
- Public demo: https://4-r-c-4-n-4.github.io/OpenXOPS-web/ (automatically deployed
  from this repository; see DEPLOYMENT.md).

## Wrong ZIP / “Data directory not exist”

`1.080-src.zip` contains source code and `1.080.zip` contains the OpenXOPS
engine; neither substitutes for the original game data. Choose `xops096.zip`.
On this machine a verified copy is at
`/home/ivy/Work/OpenXOPS-web/xopsdata/xops096.zip`.

The launcher now checks the selected archive using the engine's real ZIP/VFS
loader before entering the game. Missing representative files or damaged ZIPs
produce an actionable message and allow another selection without reloading.
A single common wrapping folder is supported. It no longer announces “Game
running” after an ordinary startup error. The legacy-GL emulation warning is
expected and is unrelated to missing game data.

## Native developer checks

The native CMake build still uses stub graphics; it is not the playable browser
renderer. It remains useful for simulation, input, VFS and audio sanitizer tests.

```sh
cmake -S . -B build-native \
  -DOPENXOPS_PLATFORM= \
  -DOPENXOPS_WINDOW_BACKEND=sdl \
  -DOPENXOPS_INPUT_BACKEND=sdl \
  -DOPENXOPS_SOUND_BACKEND=openal \
  -DOPENXOPS_GFX_BACKEND=stub \
  -DOPENXOPS_AUTOCREATECONFIG=ON \
  -DOPENXOPS_SANITIZE=ON
cmake --build build-native -j4
ctest --test-dir build-native --output-on-failure
```

See [PROGRESS.md](../PROGRESS.md) for the development history and remaining work.

## Engine/page version mismatch

The build packages JavaScript and WASM using a shared content fingerprint in
both filenames. The HTML references that exact pair, avoiding stale cached
engine APIs after updates. After HTML-only changes run
`python3 tools/package_web.py`; do not copy web/index.html directly into the
build directory (it contains build tokens). If an already-open older page
reports a missing Module function, reload to obtain the packaged page.

The browser renderer guards against backing-canvas resize on every frame;
fullscreen/DPI changes must not leave a640x480 GL viewport in a monitor-sized
buffer. Display scaling is CSS-only and preserves the logical game resolution.

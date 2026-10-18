# Play the browser preview

The actual OpenXOPS engine now boots in Chromium using the original game data:
opening → mission menu → briefing → first-person gameplay. This is an early
compatibility preview, not the finished free-data-pack release.

## On this development machine

Open **http://127.0.0.1:8766/** while the preview server is running.

1. Select `/home/ivy/Work/OpenXOPS-web/xopsdata/xops096.zip` in the file picker.
2. Click **Start game**. The archive is read locally, not uploaded to a server.
3. Press Enter to leave the opening scene.
4. Click **TRAINING YARD** in the mission menu.
5. Left-click again on the briefing to begin.
6. Click the canvas to capture the mouse. Esc releases it.

Original default controls: W/S forward/back, A/D strafe, arrow keys look,
left mouse fire, Space jump, Tab walk, R reload, Shift scope,
1/2 weapon slots. Config supplied in a modified archive may change bindings.

The original archive is gitignored and is not included in builds or hosted
files. Obtain your own copy from the distribution linked in
[NOTICE.md](../NOTICE.md) if you do not have it.

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
- Selecting the archive is required after reload. Archive/settings persistence
  is not yet implemented in the game shell (the separate smoke test has its
  own persistence test).
- Quit ends the game loop; reload the page to start another session.
- Original game data is copyrighted and must not be deployed alongside the
  engine. The CC0 replacement pack is incomplete.
- No public hosted game release is claimed by this local preview guide.

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

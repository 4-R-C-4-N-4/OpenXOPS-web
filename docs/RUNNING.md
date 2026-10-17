# Running OpenXOPS-web

## What can I boot today?

**The browser game is not playable yet.** The browser target currently available
is a toolchain smoke test: a coloured triangle, audio-buffer creation, image
codecs and persistent browser storage. It is not the OpenXOPS title screen or a
mission, and does not load game data.

The native Linux game executable runs the original simulation with real SDL
window/input, OpenAL audio and layered ZIP loading, but **graphics are still
stubbed**. A blank window is expected, not a playable result. There is no public
playable GitHub Pages release yet.

## Browser smoke test (works now)

Requirements: Python 3, a desktop browser with WebGL 2, and Emscripten **6.0.12**.
No original game assets are needed.

On this development machine, the SDK is installed at `~/programs/emsdk`:

```sh
cd ~/Work/OpenXOPS-web
source ~/programs/emsdk/emsdk_env.sh
bash tools/toolchain-smoke/build.sh web
python3 -m http.server 8765 --bind 127.0.0.1 \
  --directory tools/toolchain-smoke/build-web
```

Open **http://127.0.0.1:8765/smoke.html**. Keep the tab foregrounded for a few
seconds. Expect a coloured triangle and a log ending with:

```text
SMOKE SUMMARY: frames=120 gl=yes failures=0
```

Reload: the IDBFS log should increment `runs=1` to `runs=2`, showing that browser
storage survives reloads. Audio-buffer creation is checked; audible playback
may require a click and is not guaranteed by the log. Stop the server with
Ctrl+C. If port 8765 is occupied, use another port and change the URL.

Do not open `smoke.html` as a `file://` URL: serve it over HTTP so JavaScript can
fetch the WASM file. The server serves only the build directory, never private
`xopsdata/` files. It binds localhost, not your LAN.

For a new machine, install the pinned SDK (skip if already installed):

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/programs/emsdk
cd ~/programs/emsdk
./emsdk install 6.0.12
./emsdk activate 6.0.12
source ./emsdk_env.sh
```

## Native Linux developer build (blank graphics)

Requirements: C++17 compiler, CMake, Make, pkg-config and development libraries
for SDL2, OpenAL and zlib; iconv is supplied by libc or a separate library.
The source is UTF-8. Do not use the old CP932 text-conversion Git setting on
working source files.

```sh
cd ~/Work/OpenXOPS-web
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

Do **not** use `-DOPENXOPS_PLATFORM=sdl` yet: that shortcut also selects the
unfinished GLES game renderer. The explicit component selection above is the
verified configuration.

Obtain your own original X operations 0.96 archive from the distribution linked
in [NOTICE.md](../NOTICE.md), and place it at `xopsdata/xops096.zip` (gitignored).
The canonical archive MD5 is `020d4aa49c2501b06c6ea3e9c2df454d`. We do not bundle
or host it. For a bounded, headless data-loading check:

```sh
mkdir -p "${TMPDIR:-$HOME/.cache}/openxops-runtime"
OPENXOPS_DATA_ZIP="$PWD/xopsdata/xops096.zip" \
SDL_VIDEODRIVER=dummy ALSOFT_DRIVERS=null OPENXOPS_MAX_TICKS=100 \
  ./build-native/openxops -Dir "${TMPDIR:-$HOME/.cache}/openxops-runtime"
```

Expected: `100 ticks done, closing` and exit 0. `dummy` disables the window;
`null` tests audio without output to a speaker. Remove these two environment
variables for the normal window/audio path, but remember that rendering is
still blank. Saves default to `$XDG_DATA_HOME/openxops-web/` or
`~/.local/share/openxops-web/`; `-Dir` selects runtime data, not save storage.

## What remains before a browser game preview?

1. Implement the actual `D3DGraphics` backend: map/model loading, camera,
   textures, effects, HUD and text. The separate experimental GLES draw device
   is not wired to the game. Its pixel tests work, but an external-driver leak
   remains unresolved in leak-enabled testing.
2. Finish Japanese text decoding/font rendering.
3. Add an Emscripten game build, adapt the startup/main loop to browser
   callbacks, and port native-only VFS/save operations to browser-compatible
   filesystem APIs. The current native library is not proof of web portability.
4. Implement local archive selection, startup ordering, audio/pointer-lock
   gestures and persistent settings.
5. Test an original-data browser mission, then publish a preview. The first
   preview can require the player's original archive; completing the free pack
   must not block that engineering milestone.

A zero-upload free-pack release additionally needs original models, HUD/font
atlas, title, maps/missions, and the remaining effect/sky artwork. The current
textures, scope and sounds alone do not make a complete game.

The renderer is the largest remaining piece; this is more than a final build
flag or deployment step. See [PROGRESS.md](../PROGRESS.md) for verified status.

## Latest verification

The browser smoke was rebuilt with Emscripten 6.0.12 and exercised in Chromium:
120 frames, graphics enabled, zero reported failures; IDBFS `runs` incremented
from 1 to 2 after reload. The integrated native SDL/VFS/OpenAL build passed five
CTest tests and 100-tick offscreen, dummy and ZIP-only runs under sanitizers.
Neither result is a claim that the browser game itself is playable.

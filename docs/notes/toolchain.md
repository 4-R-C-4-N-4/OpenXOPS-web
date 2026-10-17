# Toolchain notes (Phase 0)

Verified on 2026-10-17 on Arch Linux (no sudo) for SCOPE-OF-WORK §9 / Phase 0.
The smoke test is in `tools/toolchain-smoke/`.

## Install

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/programs/emsdk
cd ~/programs/emsdk
./emsdk install latest
./emsdk activate latest
```

To use it in a shell, run `source ~/programs/emsdk/emsdk_env.sh` (set `EMSDK_QUIET=1` to
hide the banner). Do this per shell or per script. It is deliberately **not** in `~/.bashrc`.

## Versions

| Component | Version |
|---|---|
| emsdk (git) | `1c9a998ee5dd2e38aec0c5abbf23dffdb8f10d57` (2026-10-16, "Fix lint (#1809)") |
| `latest` resolves to | SDK 6.0.12 = `sdk-releases-6e9f6885cb17a3d6c8fd488a5cff730f50aba61d-64bit` |
| emcc | `emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.12 (5488e0871d46d19a3209988190fa6a653126256f)` |
| emsdk node | 24.19.0 (`$EMSDK_NODE`) |
| Ports fetched on first build | SDL 2.32.10, zlib 1.3.2, libpng 1.6.58, libjpeg 9f (`JPEG_LIB_VERSION 90`) |
| Host native | gcc 16.2.1, clang 22.1.8, cmake 4.4.3, SDL2 2.32.72 (pkg-config), OpenAL Soft 1.25.2, GLESv2 3.2 (NVIDIA 615.71.09), zlib 1.3.2, libpng 1.6.58, libjpeg 8 ABI |

The emsdk stack is pinned by the emsdk commit plus the SDK version. For CI, use
`./emsdk install 6.0.12 && ./emsdk activate 6.0.12` (or `mymindstorm/setup-emsdk` with
`version: 6.0.12`). Don't use `latest` there.

## Verified web flag set

```
-O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1
-sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 -sFULL_ES3=0
-lopenal -lidbfs.js
-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
-sEXPORTED_RUNTIME_METHODS=FS
```

**No changes were needed.** emcc 6.0.12 accepts every flag, and it prints no warnings or
deprecation notices on either a cold or a warm port cache. There is no `-pthread`, because
GitHub Pages can't send COOP/COEP headers.

Notes:
- `-sFULL_ES3=0` is already the default. It is harmless and keeps the intent explicit (we
  use a WebGL2 subset only; no client-side-array emulation).
- `-sUSE_*` port flags must also go on the **compile** step, not only the link step,
  because they add the port include paths (`SDL.h`, `png.h`, `jpeglib.h`). `em++`
  compile+link in one command does both. In CMake, add them to both
  `target_compile_options` and `target_link_options` (see below).
- `#include <SDL.h>` works on both targets (the emscripten port and pkg-config both put
  `.../SDL2` on the include path). Use `<SDL.h>`, not `<SDL2/SDL.h>`.

## Output sizes (smoke test, `-O2`)

| File | raw | gzip -9 | brotli |
|---|---:|---:|---:|
| smoke.html | 19,598 | 13,678 | 12,864 |
| smoke.js | 235,367 | 52,617 | 45,556 |
| smoke.wasm | 552,118 | 182,474 | 152,017 |

This is the baseline for SDL2, OpenAL, IDBFS, zlib, libpng and libjpeg linked together.
The same code built through CMake at `-O3` gives 224,305 B of JS and 539,981 B of wasm.
A cold build, which compiles all four ports into `~/programs/emsdk/upstream/emscripten/cache`,
takes about 7 s wall time on this machine.

## Smoke test

`tools/toolchain-smoke/build.sh [web|native|all]`. The test prints a PASS/FAIL/SKIP line
for each subsystem and ends with `SMOKE SUMMARY: frames=120 gl=… failures=N`. On web it
also sets `document.title` to `SMOKE DONE failures=N`.

Results:
- **Web (Chromium via browser automation, served by `python3 -m http.server`):** zlib,
  libpng, libjpeg, openal (`Emscripten / 1.1`), sdl (driver `emscripten`), glctx (`WebGL 2.0
  (OpenGL ES 3.0 Chromium)`), shader, draw (glReadPixels centre = 118,68,69), and IDBFS all
  PASS, with `failures=0` after 120 frames. IDBFS persisted across a reload (`runs=1`, then
  `runs=2`) and `syncfs(false)` reported PASS. The console showed no errors or warnings.
  The only failed network request was a 404 for favicon.ico.
- **Native (g++ and clang++, `-Wall -Wextra`, no warnings):**
  - `SDL_VIDEODRIVER=offscreen`: everything passes, because the EGL offscreen path gets a
    real ES 3.2 context on the NVIDIA driver.
  - `SDL_VIDEODRIVER=dummy`: the window step reports SKIP ("OpenGL support is either not
    configured…"). The program prints `GL unavailable: continuing without rendering`, runs
    all 120 frames and exits 0.
  - Default (wayland): everything passes.
  - When no audio device exists, `alcOpenDevice` is reported as SKIP rather than FAIL.

## CMake for the real project

Configure through the emscripten toolchain file. `emcmake` adds it automatically:

```bash
source ~/programs/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
# emcmake expands to:
#   cmake ... -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
#             -DCMAKE_CROSSCOMPILING_EMULATOR=$EMSDK_NODE
```

Ninja isn't installed here, and emcmake picked **Unix Makefiles**, which works. Branch on
`if(EMSCRIPTEN)`. This snippet was verified to build the smoke test:

```cmake
if(EMSCRIPTEN)
  set(OXW_PORTS "SHELL:-sUSE_SDL=2" "SHELL:-sUSE_ZLIB=1" "SHELL:-sUSE_LIBPNG=1" "SHELL:-sUSE_LIBJPEG=1")
  target_compile_options(openxops-web PRIVATE ${OXW_PORTS})
  target_link_options(openxops-web PRIVATE ${OXW_PORTS}
    "SHELL:-sMAX_WEBGL_VERSION=2" "SHELL:-sMIN_WEBGL_VERSION=2" "SHELL:-sFULL_ES3=0"
    -lopenal -lidbfs.js
    "SHELL:-sALLOW_MEMORY_GROWTH=1" "SHELL:-sINITIAL_MEMORY=64MB"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=FS")
  set_target_properties(openxops-web PROPERTIES SUFFIX ".html")   # or .js with a custom shell
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET sdl2 openal glesv2 zlib libpng libjpeg)
  target_link_libraries(openxops PRIVATE PkgConfig::DEPS)
endif()
```

Wrap every `-s` option in `SHELL:`. Without it, CMake de-duplicates the repeated `-s` tokens
and breaks the flags.

## Gotchas found

1. **CMake `Release` means `-O3`, not `-O2`.** To keep the SOW's `-O2`, set
   `CMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"` (and the C equivalent), or add `-O2` to the link
   options. emcc runs wasm-opt at the link-time `-O` level.
2. **Hidden tabs never tick.** `emscripten_set_main_loop(step, 0, 1)` uses
   requestAnimationFrame, and a background or headless tab (`visibilityState=hidden`) gets
   **zero** frames. In the first automated run only frame 0 executed. Bring the tab to the
   front for tests. The game should also pause cleanly on `visibilitychange`, and the
   accumulator clamp from the SOW still applies.
3. **IDBFS is async.** Mount it and run `FS.syncfs(true, cb)`, and touch `/save` only after
   the callback fires. After a write, call `FS.syncfs(false, cb)` or the data is lost on
   reload. The smoke test calls back into C with `Module._smoke_idbfs_ready` plus
   `EMSCRIPTEN_KEEPALIVE`. `-sEXPORTED_RUNTIME_METHODS=FS` exposes `Module.FS`, but the
   `EM_ASM` code in the module can use `FS` / `IDBFS` directly.
4. **OpenAL on web** creates its context and buffers without a user gesture, but the
   AudioContext stays suspended until one happens, so playback starts silent. Keep the SOW's
   "Click to play" gate.
5. **libjpeg ABI differs.** The port is IJG 9f (`JPEG_LIB_VERSION 90`) and the host has a
   libjpeg-turbo 8 ABI. Both work for decode-only use, but don't rely on 9-only APIs.
6. The host `SDL2 2.32.72` is sdl2-compat on SDL3, while the web port is real SDL 2.32.10.
   Their behaviour should match for our API subset.
7. `emsdk_env.sh` doesn't put emsdk's node first in `PATH` when nvm is already active
   (`which node` → nvm v22). emcc uses `$EMSDK_NODE` (v24) internally, so this is harmless,
   but run tests with `$EMSDK_NODE` if node versions matter.
8. Native headless: `SDL_VIDEODRIVER=offscreen` gives real GL through EGL on this NVIDIA
   box, so prefer it to `dummy` for headless render tests (CI runners without a GPU will
   fall back to the SKIP path).

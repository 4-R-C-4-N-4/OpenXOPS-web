# cmake/modules

Every `*.cmake` file here is included, in sorted order, after the `openxops` target is created
(see the top-level CMakeLists.txt). One file per component or feature keeps parallel work
merge-free:

| File | Owner |
|---|---|
| `20-window-sdl.cmake`, `21-input-sdl.cmake`, `22-sound-openal.cmake` | platform shell (SDL2/OpenAL) |
| `30-gfx-gles.cmake` | GLES3/WebGL2 renderer |
| `10-vfs.cmake` | virtual file system |
| `40-text.cmake` | CP932 text + glyph atlas |
| `90-web.cmake` | Emscripten target (Phase 2) |

Each module checks its `OPENXOPS_*_BACKEND` variable, uses `target_sources(openxops PRIVATE ...)`,
and may add its own test executable plus `add_test()`.

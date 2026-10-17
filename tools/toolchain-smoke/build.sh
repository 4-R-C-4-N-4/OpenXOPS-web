#!/usr/bin/env bash
# OpenXOPS-web toolchain smoke test build.
#   ./build.sh web      -> build-web/smoke.{html,js,wasm}   (needs: source ~/programs/emsdk/emsdk_env.sh)
#   ./build.sh native   -> build-native/smoke               (system SDL2/OpenAL/GLESv2/zlib/libpng/libjpeg)
#   ./build.sh          -> both
set -euo pipefail
cd "$(dirname "$0")"

# The verified web flag set from the scope of work (see docs/notes/toolchain.md).
WEB_FLAGS=(
  -O2
  -sUSE_SDL=2
  -sUSE_ZLIB=1
  -sUSE_LIBPNG=1
  -sUSE_LIBJPEG=1
  -sMAX_WEBGL_VERSION=2
  -sMIN_WEBGL_VERSION=2
  -sFULL_ES3=0
  -lopenal
  -lidbfs.js
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=64MB
  -sEXPORTED_RUNTIME_METHODS=FS
)

build_web() {
  command -v em++ >/dev/null || { echo "em++ not found: source ~/programs/emsdk/emsdk_env.sh" >&2; exit 1; }
  mkdir -p build-web
  em++ main.cpp "${WEB_FLAGS[@]}" -o build-web/smoke.html
  ls -l build-web/
}

build_native() {
  local cxx="${CXX:-c++}"
  mkdir -p build-native
  # libjpeg has no .pc on some distros; fall back to -ljpeg
  local jpeg; jpeg=$(pkg-config --cflags --libs libjpeg 2>/dev/null || echo -ljpeg)
  "$cxx" -O2 -g -Wall -Wextra -std=c++17 main.cpp -o build-native/smoke \
    $(pkg-config --cflags --libs sdl2 openal glesv2 zlib libpng) $jpeg
  ls -l build-native/
}

case "${1:-all}" in
  web) build_web ;;
  native) build_native ;;
  all) build_native; build_web ;;
  *) echo "usage: $0 [web|native|all]" >&2; exit 2 ;;
esac

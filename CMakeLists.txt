cmake_minimum_required(VERSION 3.16)
project(OpenXOPS-web LANGUAGES CXX)

# OpenXOPS-web native build.
# Phase 0: upstream game logic + stub platform classes (no window, input,
# sound or rendering). Real SDL2/OpenAL/GLES backends come in Phase 1.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(OPENXOPS_SANITIZE "Build with -fsanitize=address,undefined" OFF)
option(OPENXOPS_AUTOCREATECONFIG "Define ENABLE_AUTOCREATECONFIG (write a default config.dat if missing)" OFF)
option(OPENXOPS_PATH_DELIMITER_SLASH "Define ENABLE_PATH_DELIMITER_SLASH (convert '\\\\' to '/' in data paths)" ON)
set(OPENXOPS_PLATFORM "stub" CACHE STRING "Platform backend set (Phase 0: stub only)")
set_property(CACHE OPENXOPS_PLATFORM PROPERTY STRINGS stub)

if(WIN32)
  message(FATAL_ERROR "This CMake build targets non-Windows platforms. Build upstream Windows "
                      "backends with the upstream Visual C++ project (add src/platform/common.cpp "
                      "and src/platform/input-common.cpp to it).")
endif()

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Upstream game logic (platform-neutral). Not compiled here: d3dgraphics-directx.cpp,
# d3dgraphics-opengl.cpp, sound-ezds.cpp, sound-directsound.cpp, input.cpp, window.cpp.
set(OPENXOPS_GAME_SOURCES
  ${SRC}/ai.cpp
  ${SRC}/collision.cpp
  ${SRC}/config.cpp
  ${SRC}/d3dgraphics-debugfont.cpp
  ${SRC}/datafile.cpp
  ${SRC}/debug.cpp
  ${SRC}/event.cpp
  ${SRC}/gamemain.cpp
  ${SRC}/gui-object.cpp
  ${SRC}/main.cpp
  ${SRC}/object.cpp
  ${SRC}/objectmanager.cpp
  ${SRC}/parameter.cpp
  ${SRC}/resource.cpp
  ${SRC}/scene.cpp
  ${SRC}/soundmanager.cpp
  ${SRC}/statemachine.cpp
)

# Upstream helpers moved out of window.cpp / input.cpp (shared by all backends).
set(OPENXOPS_COMMON_SOURCES
  ${SRC}/platform/common.cpp
  ${SRC}/platform/input-common.cpp
)

if(OPENXOPS_PLATFORM STREQUAL "stub")
  set(OPENXOPS_PLATFORM_SOURCES
    ${SRC}/platform/stub/window-stub.cpp
    ${SRC}/platform/stub/input-stub.cpp
    ${SRC}/platform/stub/sound-stub.cpp
    ${SRC}/platform/stub/d3dgraphics-stub.cpp
  )
  set(OPENXOPS_PLATFORM_DEFS OPENXOPS_PLATFORM_STUB)
else()
  message(FATAL_ERROR "Unknown OPENXOPS_PLATFORM '${OPENXOPS_PLATFORM}'")
endif()

add_executable(openxops
  ${OPENXOPS_GAME_SOURCES}
  ${OPENXOPS_COMMON_SOURCES}
  ${OPENXOPS_PLATFORM_SOURCES}
)

target_include_directories(openxops PRIVATE ${SRC})
target_compile_definitions(openxops PRIVATE ${OPENXOPS_PLATFORM_DEFS})
if(OPENXOPS_AUTOCREATECONFIG)
  target_compile_definitions(openxops PRIVATE ENABLE_AUTOCREATECONFIG)
endif()
if(OPENXOPS_PATH_DELIMITER_SLASH)
  target_compile_definitions(openxops PRIVATE ENABLE_PATH_DELIMITER_SLASH)
endif()

target_compile_options(openxops PRIVATE -Wall -Wextra)
target_link_libraries(openxops PRIVATE m)

if(OPENXOPS_SANITIZE)
  target_compile_options(openxops PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(openxops PRIVATE -fsanitize=address,undefined)
endif()

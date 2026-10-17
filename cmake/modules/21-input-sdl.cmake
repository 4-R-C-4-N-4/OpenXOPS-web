if(OPENXOPS_INPUT_BACKEND STREQUAL "sdl")
  if(NOT OPENXOPS_WINDOW_BACKEND STREQUAL "sdl")
    message(FATAL_ERROR "SDL input requires OPENXOPS_WINDOW_BACKEND=sdl")
  endif()
  target_sources(openxops PRIVATE ${SRC}/platform/sdl/input-sdl.cpp)
  target_compile_definitions(openxops PRIVATE OPENXOPS_INPUT_SDL)
  add_executable(sdl-shell-test ${CMAKE_CURRENT_SOURCE_DIR}/tests/sdl-shell-test.cpp
    ${SRC}/platform/sdl/window-sdl.cpp ${SRC}/platform/sdl/input-sdl.cpp
    ${SRC}/platform/input-common.cpp ${SRC}/platform/common.cpp ${SRC}/debug.cpp)
  target_include_directories(sdl-shell-test PRIVATE ${SRC})
  target_compile_definitions(sdl-shell-test PRIVATE OPENXOPS_INPUT_SDL OPENXOPS_WINDOW_SDL OPENXOPS_PLATFORM_STUB)
  target_link_libraries(sdl-shell-test PRIVATE PkgConfig::SDL2)
  if(OPENXOPS_SANITIZE)
    target_compile_options(sdl-shell-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(sdl-shell-test PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME sdl-input-dummy COMMAND sdl-shell-test dummy)
  set_tests_properties(sdl-input-dummy PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  add_test(NAME sdl-gles-offscreen COMMAND sdl-shell-test gl)
  set_tests_properties(sdl-gles-offscreen PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
  # Prefer Mesa software EGL for deterministic CI; proprietary drivers may retain
  # process-global allocations which LeakSanitizer reports after context teardown.
  if(EXISTS "/usr/share/glvnd/egl_vendor.d/50_mesa.json")
    set_property(TEST sdl-gles-offscreen APPEND PROPERTY ENVIRONMENT
      "__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json")
  endif()
endif()

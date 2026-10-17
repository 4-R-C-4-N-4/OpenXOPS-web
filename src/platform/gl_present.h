//! @file gl_present.h
//! @brief [OpenXOPS-web] Contract between the window backend and the GL renderer.
//!
//! The window backend (src/platform/sdl/window-sdl.cpp) owns the native window and the
//! OpenGL ES 3.0 / WebGL 2 context, and implements these functions. The renderer
//! (src/platform/gles/d3dgraphics-gles.cpp) calls them and never touches SDL directly.
//! The stub window backend implements them as no-ops (GL unavailable).
#ifndef PLATFORM_GL_PRESENT_H
#define PLATFORM_GL_PRESENT_H

//! True when a GLES 3.0 (native) / WebGL 2 (web) context exists and is current on this thread.
bool Platform_GLAvailable();

//! Present the back buffer (SDL_GL_SwapWindow). Called once per frame from D3DGraphics::EndRender().
void Platform_GLSwap();

//! Current drawable size in pixels (may differ from the logical game resolution on HiDPI / web canvas).
void Platform_GLDrawableSize(int *w, int *h);

#endif

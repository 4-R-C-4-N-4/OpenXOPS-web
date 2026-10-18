// Copyright (c) 2026 OpenXOPS-web contributors. BSD-3-Clause.
#include "../../window.h"
#include "../gl_present.h"
#include "events-sdl.h"
#include <cerrno>
#include <climits>
static SDL_Window* window = nullptr;
static SDL_GLContext context = nullptr;
static bool focused = true;
static bool initialized = false;
SDL_Window* Platform_SDLWindow(){ return window; }
WindowControl::WindowControl(): width(0), height(0), fullscreenflag(false), closeflag(false), frames(0) {}
static void DestroySDLWindow(){
    if(!initialized) return;
    initialized = false;
    if(context) SDL_GL_DeleteContext(context);
    context = nullptr;
    if(window) SDL_DestroyWindow(window);
    window = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
    if(SDL_WasInit(0) == 0) SDL_Quit();
}
WindowControl::~WindowControl(){ DestroySDLWindow(); }
bool WindowControl::InitWindow(const char* title, int w, int h, bool fullscreen){
    if(!title || w <= 0 || h <= 0 || window) return false;
    if(SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0){ ErrorInfo(SDL_GetError()); return false; }
    initialized = true;
    const bool dummy = !strcmp(SDL_GetCurrentVideoDriver(), "dummy");
#ifndef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_ALLOW_HIGHDPI | (dummy ? 0 : SDL_WINDOW_OPENGL));
    if(!window){ ErrorInfo(SDL_GetError()); DestroySDLWindow(); return false; }
    if(!dummy){
        context = SDL_GL_CreateContext(window);
        if(!context){ ErrorInfo(SDL_GetError()); DestroySDLWindow(); return false; }
#ifndef __EMSCRIPTEN__
        SDL_GL_SetSwapInterval(0);
#endif
        fprintf(stderr, "[sdl] GLES3 context created (%s)\n", SDL_GetCurrentVideoDriver());
    }else fprintf(stderr, "[sdl] dummy video: no GL context\n");
    // MainWindow is global: release GL before SDL/driver atexit handlers run.
    if(atexit(DestroySDLWindow) != 0){ DestroySDLWindow(); return false; }
    width = w; height = h; closeflag = false; frames = 0; focused = true;
#ifdef __EMSCRIPTEN__
    return true; // Browser fullscreen requires an explicit user gesture.
#else
    return ChangeWindowMode(fullscreen);
#endif
}
bool WindowControl::ChangeWindowMode(bool fullscreen){
    if(!window) return false;
    if(SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0){ ErrorInfo(SDL_GetError()); return false; }
    fullscreenflag = fullscreen;
    return true;
}
int WindowControl::CheckMainLoop(){
#ifdef OPENXOPS_INPUT_SDL
    Platform_SDLBeginEventPump();
#endif
    SDL_Event e;
    while(SDL_PollEvent(&e)){
#ifdef OPENXOPS_INPUT_SDL
        Platform_SDLInputEvent(e);
#endif
        if(e.type == SDL_QUIT) closeflag = true;
        if(e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(window)){
            if(e.window.event == SDL_WINDOWEVENT_CLOSE) closeflag = true;
            if(e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) focused = false;
            if(e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
        }
    }
    if(closeflag) return -1;
    const char* limit = getenv("OPENXOPS_MAX_TICKS");
    if(limit){
        char* end; errno = 0; unsigned long n = strtoul(limit, &end, 10);
        if(!errno && end != limit && !*end && *limit != '-' && n <= UINT_MAX && frames >= n){
            fprintf(stderr, "[sdl] %u ticks done, closing\n", frames); closeflag = true; return -1;
        }
    }
    if(!focused){ SDL_Delay(10); return 0; }
    ++frames;
    return 1;
}
void WindowControl::ErrorInfo(const char* text){
    if(!text) return;
    fprintf(stderr, "[sdl] error: %s\n", text);
    if(window && strcmp(SDL_GetCurrentVideoDriver(), "dummy") && strcmp(SDL_GetCurrentVideoDriver(), "offscreen"))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenXOPS error", text, window);
}
void WindowControl::CloseWindow(){ closeflag = true; }
unsigned int GetTimeMS(){ return SDL_GetTicks(); }
#ifdef ENABLE_DEBUGLOG
void GetOperatingEnvironment(){ OutputLog.WriteLog(LOG_CHECK, "Environment", SDL_GetPlatform()); }
#endif
bool Platform_GLAvailable(){ return context && SDL_GL_GetCurrentContext() == context; }
void Platform_GLSwap(){ if(Platform_GLAvailable()) SDL_GL_SwapWindow(window); }
void Platform_GLDrawableSize(int* w, int* h){
    int x = 0, y = 0;
    if(Platform_GLAvailable()) SDL_GL_GetDrawableSize(window, &x, &y);
    if(w) *w = x;
    if(h) *h = y;
}

// Copyright (c) 2026 OpenXOPS-web contributors. BSD-3-Clause.
#include "input.h"
#include "platform/sdl/events-sdl.h"
#include "platform/gl_present.h"
#include <GLES3/gl3.h>
#include <cstdlib>
#define REQUIRE(x) do { if(!(x)){ fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
DebugLog OutputLog;
// Match the game's global window lifetime (SDL/GL atexit ordering matters).
WindowControl w;
static void key(Uint32 type, SDL_Scancode scan){
    SDL_Event e{}; e.type = type; e.key.windowID = SDL_GetWindowID(Platform_SDLWindow()); e.key.keysym.scancode = scan;
    REQUIRE(SDL_PushEvent(&e) == 1);
}
int main(int argc, char** argv){
    REQUIRE(!w.InitWindow(nullptr, 64, 64, false));
    REQUIRE(w.InitWindow("SDL shell test", 64, 64, false));
    bool gl = argc > 1 && !strcmp(argv[1], "gl");
    REQUIRE(Platform_GLAvailable() == gl);
    int width, height; Platform_GLDrawableSize(&width,&height);
    if(gl){
        REQUIRE(width > 0 && height > 0);
        using GetString = const GLubyte* (*)(GLenum);
        using ClearColor = void (*)(GLfloat,GLfloat,GLfloat,GLfloat);
        using Clear = void (*)(GLbitfield);
        using Read = void (*)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
        auto get = (GetString)SDL_GL_GetProcAddress("glGetString");
        auto color = (ClearColor)SDL_GL_GetProcAddress("glClearColor");
        auto clear = (Clear)SDL_GL_GetProcAddress("glClear");
        auto read = (Read)SDL_GL_GetProcAddress("glReadPixels");
        REQUIRE(get && color && clear && read);
        fprintf(stderr,"GL: %s\n",get(GL_VERSION));
        REQUIRE(strstr((const char*)get(GL_VERSION),"OpenGL ES 3"));
        color(1,0,0,1); clear(GL_COLOR_BUFFER_BIT);
        unsigned char pixel[4]{}; read(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        REQUIRE(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0); Platform_GLSwap();
    }else REQUIRE(width == 0 && height == 0);
    InputControl input; REQUIRE(input.InitInput(&w) == 0);
    REQUIRE(w.CheckMainLoop() == 1); input.GetInputState(false);
    // Every configurable original keyboard code must have a scancode mapping.
    for(int i=0;i<=0x4a;++i){
        int vk=OriginalkeycodeToDinputdef(i); if(vk < 0 || vk == VK_SHIFT || vk == VK_CONTROL) continue;
        bool found=false; for(int s=1;s<SDL_NUM_SCANCODES;++s) found |= Platform_ScancodeToVK(SDL_Scancode(s)) == vk;
        REQUIRE(found);
    }
    REQUIRE(Platform_ScancodeToVK(SDL_SCANCODE_UNKNOWN) == 0);
    REQUIRE(Platform_ScancodeToVK(SDL_SCANCODE_ESCAPE) == GetEscKeycode());
    for(int i=1;i<=12;++i) REQUIRE(Platform_ScancodeToVK(SDL_Scancode(SDL_SCANCODE_F1+i-1)) == GetFunctionKeycode(i));
    key(SDL_KEYDOWN,SDL_SCANCODE_A); key(SDL_KEYDOWN,SDL_SCANCODE_LSHIFT); key(SDL_KEYDOWN,SDL_SCANCODE_RSHIFT);
    REQUIRE(w.CheckMainLoop() == 1); input.GetInputState(true);
    REQUIRE(input.CheckKeyDown('A') && input.CheckKeyNow(VK_SHIFT));
    input.GetInputState(true); REQUIRE(!input.CheckKeyDown('A'));
    key(SDL_KEYUP,SDL_SCANCODE_A); key(SDL_KEYUP,SDL_SCANCODE_LSHIFT);
    SDL_Event e{}; e.type=SDL_MOUSEMOTION; e.motion.windowID=SDL_GetWindowID(Platform_SDLWindow()); e.motion.x=23;e.motion.y=42;e.motion.xrel=400;e.motion.yrel=-500; REQUIRE(SDL_PushEvent(&e)==1);
    e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=SDL_GetWindowID(Platform_SDLWindow());e.button.button=SDL_BUTTON_LEFT;REQUIRE(SDL_PushEvent(&e)==1);
    REQUIRE(w.CheckMainLoop()==1);input.GetInputState(true);
    int x,y;input.GetMouseMovement(&x,&y);REQUIRE(x==127 && y==-128);
    REQUIRE(input.CheckKeyUp('A') && input.CheckKeyNow(VK_SHIFT) && input.CheckMouseButtonDownL());
    // Absolute-mode menu polling must not consume pending relative motion: a
    // transition to gameplay in this same event batch still gets its delta.
    e={}; e.type=SDL_MOUSEMOTION; e.motion.windowID=SDL_GetWindowID(Platform_SDLWindow());
    e.motion.x=24; e.motion.y=43; e.motion.xrel=7; e.motion.yrel=-9; REQUIRE(SDL_PushEvent(&e)==1);
    e.motion.x=25; e.motion.y=44; e.motion.xrel=3; e.motion.yrel=4; REQUIRE(SDL_PushEvent(&e)==1);
    REQUIRE(w.CheckMainLoop()==1);
    input.GetInputState(false); input.GetMouseMovement(&x,&y); REQUIRE(x==25 && y==44);
    input.GetInputState(false); input.GetMouseMovement(&x,&y); REQUIRE(x==25 && y==44);
    input.GetInputState(true); input.GetMouseMovement(&x,&y); REQUIRE(x==10 && y==-5);
    input.GetInputState(true); input.GetMouseMovement(&x,&y); REQUIRE(x==0 && y==0);
    input.GetInputState(false); input.GetMouseMovement(&x,&y); REQUIRE(x==25 && y==44);
    e={};e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(Platform_SDLWindow());e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;REQUIRE(SDL_PushEvent(&e)==1);
    REQUIRE(w.CheckMainLoop()==0);input.GetInputState(true);REQUIRE(input.CheckKeyUp(VK_SHIFT) && input.CheckMouseButtonUpL());
    e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;REQUIRE(SDL_PushEvent(&e)==1);REQUIRE(w.CheckMainLoop()==1);
    input.SetMouseLimitFlag(false);
    e={};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(Platform_SDLWindow());e.motion.xrel=300;e.motion.yrel=-400;REQUIRE(SDL_PushEvent(&e)==1);
    e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=SDL_GetWindowID(Platform_SDLWindow());e.button.button=SDL_BUTTON_RIGHT;REQUIRE(SDL_PushEvent(&e)==1);
    key(SDL_KEYDOWN,SDL_SCANCODE_LCTRL);key(SDL_KEYDOWN,SDL_SCANCODE_RCTRL);
    REQUIRE(w.CheckMainLoop()==1);input.GetInputState(true);input.GetMouseMovement(&x,&y);
    REQUIRE(x==300 && y==-400 && input.CheckMouseButtonDownR() && input.CheckKeyDown(VK_CONTROL));
    key(SDL_KEYUP,SDL_SCANCODE_LCTRL);REQUIRE(w.CheckMainLoop()==1);input.GetInputState(true);REQUIRE(input.CheckKeyNow(VK_CONTROL));
    key(SDL_KEYUP,SDL_SCANCODE_RCTRL);
    e.type=SDL_MOUSEBUTTONUP;REQUIRE(SDL_PushEvent(&e)==1);REQUIRE(w.CheckMainLoop()==1);input.GetInputState(true);
    REQUIRE(input.CheckMouseButtonUpR() && input.CheckKeyUp(VK_CONTROL));
    REQUIRE(w.ChangeWindowMode(true)); REQUIRE(w.ChangeWindowMode(false));
    input.DestroyInput(); e={};e.type=SDL_QUIT;REQUIRE(SDL_PushEvent(&e)==1);REQUIRE(w.CheckMainLoop()==-1);
    fprintf(stderr,"SDL shell tests passed\n");
}

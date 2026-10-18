// Copyright (c) 2026 OpenXOPS-web contributors. BSD-3-Clause.
#include "../../input.h"
#include "events-sdl.h"
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
static SDLInputSnapshot state;
void Platform_SDLResetInput(){ state = SDLInputSnapshot{}; }
void Platform_SDLBeginEventPump(){ state.dx = state.dy = 0; }
int Platform_ScancodeToVK(SDL_Scancode s){
    if(s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return 'A' + s - SDL_SCANCODE_A;
    if(s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return '1' + s - SDL_SCANCODE_1;
    if(s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) return VK_F1 + s - SDL_SCANCODE_F1;
    if(s >= SDL_SCANCODE_KP_1 && s <= SDL_SCANCODE_KP_9) return VK_NUMPAD1 + s - SDL_SCANCODE_KP_1;
    switch(s){
#define MAP(a,b) case SDL_SCANCODE_##a: return b
        MAP(0,'0'); MAP(KP_0,VK_NUMPAD0); MAP(UP,VK_UP); MAP(DOWN,VK_DOWN);
        MAP(LEFT,VK_LEFT); MAP(RIGHT,VK_RIGHT); MAP(BACKSPACE,VK_BACK);
        MAP(RETURN,VK_RETURN); MAP(KP_ENTER,VK_RETURN); MAP(TAB,VK_TAB);
        MAP(SPACE,VK_SPACE); MAP(ESCAPE,VK_ESCAPE); MAP(HOME,VK_HOME);
        MAP(LSHIFT,VK_LSHIFT); MAP(RSHIFT,VK_RSHIFT);
        MAP(LCTRL,VK_LCONTROL); MAP(RCTRL,VK_RCONTROL);
        MAP(SEMICOLON,VK_OEM_1); MAP(EQUALS,VK_OEM_PLUS); MAP(MINUS,VK_OEM_MINUS);
        MAP(SLASH,VK_OEM_2); MAP(GRAVE,VK_OEM_3); MAP(LEFTBRACKET,VK_OEM_4);
        MAP(BACKSLASH,VK_OEM_5); MAP(RIGHTBRACKET,VK_OEM_6); MAP(APOSTROPHE,VK_OEM_7);
        MAP(NONUSBACKSLASH,VK_OEM_102); MAP(INTERNATIONAL1,VK_OEM_102); MAP(INTERNATIONAL3,VK_OEM_5);
        MAP(COMMA,VK_OEM_COMMA); MAP(PERIOD,VK_OEM_PERIOD);
        MAP(KP_MULTIPLY,VK_MULTIPLY); MAP(KP_DIVIDE,VK_DIVIDE);
        MAP(KP_PLUS,VK_ADD); MAP(KP_MINUS,VK_SUBTRACT); MAP(KP_PERIOD,VK_DECIMAL);
#undef MAP
        default: return 0;
    }
}
void Platform_SDLInputEvent(const SDL_Event& e){
    SDL_Window* w = Platform_SDLWindow();
    if(!w) return;
    Uint32 id = SDL_GetWindowID(w);
    if(e.type == SDL_WINDOWEVENT && e.window.windowID == id && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST){ Platform_SDLResetInput(); SDL_SetRelativeMouseMode(SDL_FALSE); }
    if((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.windowID == id){
        int s = e.key.keysym.scancode;
        if(s > 0 && s < SDL_NUM_SCANCODES) state.scans[s] = e.type == SDL_KEYDOWN;
    }
    if(e.type == SDL_MOUSEMOTION && e.motion.windowID == id){
        state.x = e.motion.x; state.y = e.motion.y;
        // Clamp accumulation to prevent signed overflow on pathological queues.
        state.dx = int(std::clamp((long long)state.dx + e.motion.xrel, -2147483647LL, 2147483647LL));
        state.dy = int(std::clamp((long long)state.dy + e.motion.yrel, -2147483647LL, 2147483647LL));
    }
    if((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.windowID == id){
        if(e.button.button == SDL_BUTTON_LEFT) state.left = e.type == SDL_MOUSEBUTTONDOWN;
        if(e.button.button == SDL_BUTTON_RIGHT) state.right = e.type == SDL_MOUSEBUTTONDOWN;
    }
}
InputControl::InputControl(): InitFlag(false), MouseLimitFlag(true), keys{}, keys_lt{}, mx(0), my(0), mbl(false), mbr(false), mbl_lt(false), mbr_lt(false) {}
InputControl::~InputControl(){ DestroyInput(); }
int InputControl::InitInput(WindowControl* w){
    if(!w || !Platform_SDLWindow()) return 1;
    Platform_SDLResetInput(); InitFlag = true; SDL_ShowCursor(SDL_DISABLE); return 0;
}
void InputControl::DestroyInput(){
    if(!InitFlag) return;
    SDL_SetRelativeMouseMode(SDL_FALSE); SDL_ShowCursor(SDL_ENABLE);
    Platform_SDLResetInput(); InitFlag = false;
}
void InputControl::SetMouseLimitFlag(bool flag){ MouseLimitFlag = flag; }
void InputControl::GetInputState(bool relative){
    if(!InitFlag) return;
    memcpy(keys_lt, keys, sizeof(keys)); memset(keys, 0, sizeof(keys));
    for(int s = 1; s < SDL_NUM_SCANCODES; ++s){
        int vk = Platform_ScancodeToVK(SDL_Scancode(s));
        if(vk && state.scans[s]) keys[vk] = char(0x80);
    }
    keys[VK_SHIFT] = keys[VK_LSHIFT] | keys[VK_RSHIFT];
    keys[VK_CONTROL] = keys[VK_LCONTROL] | keys[VK_RCONTROL];
    mbl_lt = mbl; mbr_lt = mbr; mbl = state.left; mbr = state.right;
    keys[VK_LBUTTON] = mbl ? char(0x80) : 0; keys[VK_RBUTTON] = mbr ? char(0x80) : 0;
    mx = relative ? state.dx : state.x; my = relative ? state.dy : state.y;
#ifdef __EMSCRIPTEN__
    EM_ASM({Module.input=Array.of($0,$1,$2,$3,$4);},relative,mx,my,(int)keys['W'],(int)mbl);
#endif
    // Absolute-mode menu polls must not discard deltas queued before a
    // same-frame transition to gameplay. Relative polls consume them once.
    if(relative) state.dx = state.dy = 0;
    if(relative && MouseLimitFlag){ mx = std::clamp(mx, -128, 127); my = std::clamp(my, -128, 127); }
    // Only change mode on transitions: setting it flushes pending motion in SDL.
    SDL_bool mode = relative && (SDL_GetWindowFlags(Platform_SDLWindow()) & SDL_WINDOW_INPUT_FOCUS) ? SDL_TRUE : SDL_FALSE;
    if(SDL_GetRelativeMouseMode() != mode) SDL_SetRelativeMouseMode(mode);
}
void InputControl::MoveMouseCenter(){
    if(!InitFlag) return;
    int w, h; SDL_GetWindowSize(Platform_SDLWindow(), &w, &h);
    SDL_WarpMouseInWindow(Platform_SDLWindow(), w/2, h/2);
    state.x = w/2; state.y = h/2; state.dx = state.dy = 0;
}

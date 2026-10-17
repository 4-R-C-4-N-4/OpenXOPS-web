// Copyright (c) 2026 OpenXOPS-web contributors. BSD-3-Clause.
#pragma once
#include <SDL.h>
// Single-window backend. Window polling forwards input without stealing events.
struct SDLInputSnapshot {
    bool scans[SDL_NUM_SCANCODES] = {};
    bool left = false, right = false;
    int x = 0, y = 0, dx = 0, dy = 0;
};
SDL_Window* Platform_SDLWindow();
void Platform_SDLInputEvent(const SDL_Event& event);
void Platform_SDLResetInput();
int Platform_ScancodeToVK(SDL_Scancode scan);

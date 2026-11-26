// Mario Kart DS NX - placing and drawing the two DS screens
#pragma once

#include <SDL2/SDL.h>

#include "config.h"

struct ScreenRects
{
    SDL_Rect top;
    SDL_Rect bottom;
    bool     show_top;
    bool     show_bottom;
};

// Where the screens go for a given output size and settings. `swapped`
// is the Swap Screens toggle: in FOCUS it picks which screen is big, in
// SIDE BY SIDE / STACKED it swaps their places, in ONE SCREEN it picks
// which one is shown.
ScreenRects video_layout(const Settings& cfg, int out_w, int out_h, bool swapped);

bool video_init(SDL_Renderer* r);
void video_quit();

// Uploads the newest core frame (no-op if it has not changed).
void video_upload();

// Draws both screens, the touch cursor (cursor_x/y in DS pixels, or -1 to
// hide it) and the FPS counter.
void video_draw(const Settings& cfg, const ScreenRects& rc, int cursor_x, int cursor_y, float fps);

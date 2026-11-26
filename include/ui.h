// Mario Kart DS NX - drawing helpers (text, boxes, toasts)
//
// All coordinates are in 720p units; ui_scale() converts to the real
// output size so menus look the same at 720p and 1080p.
#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace Col
{
    static const SDL_Color White  = {255, 255, 255, 255};
    static const SDL_Color Grey   = {150, 150, 165, 255};
    static const SDL_Color Dim    = {90, 90, 105, 255};
    static const SDL_Color Red    = {232, 48, 48, 255};
    static const SDL_Color Yellow = {255, 214, 40, 255};
    static const SDL_Color Green  = {90, 220, 110, 255};
    static const SDL_Color Blue   = {70, 150, 255, 255};
    static const SDL_Color Bg     = {16, 16, 28, 255};
    static const SDL_Color Panel  = {30, 30, 48, 255};
}

enum class Align { Left, Center, Right };

bool ui_init(SDL_Renderer* r, int out_w, int out_h);
void ui_resize(int out_w, int out_h);   // reopens fonts at the new size
void ui_quit();

int   ui_width();    // output size in real pixels
int   ui_height();
float ui_scale();

// size: 1 = small (help text), 2 = normal (menus), 3 = big, 5 = title
void ui_text(const std::string& s, int x, int y, int size, SDL_Color c, Align a = Align::Left);
int  ui_text_width(const std::string& s, int size);
void ui_fill(int x, int y, int w, int h, SDL_Color c);
void ui_frame(int x, int y, int w, int h, SDL_Color c);   // 2px outline
void ui_clear(SDL_Color c);

// Short message in the bottom centre that fades after ~2 seconds.
void ui_toast(const std::string& msg);
void ui_draw_toast();

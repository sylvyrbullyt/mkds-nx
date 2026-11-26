// Mario Kart DS NX - drawing helpers (text, boxes, toasts)
#include "ui.h"

#include <SDL2/SDL_ttf.h>

#include <cstdio>
#include <list>
#include <unordered_map>

#ifndef FONT_PATH   // tools/preview.cpp points this at the repo copy
#define FONT_PATH "romfs:/fonts/PressStart2P-Regular.ttf"
#endif

static const int kFontPx[] = {0, 12, 18, 26, 0, 44};   // 720p sizes per size id
static const int kFontCount = sizeof(kFontPx) / sizeof(kFontPx[0]);
static const size_t kCacheMax = 256;

static SDL_Renderer* g_r = nullptr;
static TTF_Font*     g_fonts[kFontCount] = {};
static int           g_w = 1280, g_h = 720;
static float         g_scale = 1.0f;

static std::string g_toast;
static Uint32      g_toast_until = 0;

// Rendered strings are cached: menus redraw every frame but rarely change.
struct TextEntry
{
    SDL_Texture* tex;
    int w, h;
};
static std::unordered_map<std::string, TextEntry> g_cache;
static std::list<std::string>                     g_lru;

static void cache_clear()
{
    for (auto& kv : g_cache) SDL_DestroyTexture(kv.second.tex);
    g_cache.clear();
    g_lru.clear();
}

static void fonts_close()
{
    for (TTF_Font*& f : g_fonts)
        if (f) { TTF_CloseFont(f); f = nullptr; }
}

static bool fonts_open()
{
    fonts_close();
    for (int i = 0; i < kFontCount; i++)
    {
        if (!kFontPx[i]) continue;
        g_fonts[i] = TTF_OpenFont(FONT_PATH, (int)(kFontPx[i] * g_scale + 0.5f));
        if (!g_fonts[i])
        {
            printf("UI: could not open font: %s\n", TTF_GetError());
            return false;
        }
    }
    return true;
}

bool ui_init(SDL_Renderer* r, int out_w, int out_h)
{
    g_r = r;
    if (TTF_Init() != 0) return false;
    g_w = out_w;
    g_h = out_h;
    g_scale = out_h / 720.0f;
    return fonts_open();
}

void ui_resize(int out_w, int out_h)
{
    if (out_w == g_w && out_h == g_h) return;
    g_w = out_w;
    g_h = out_h;
    g_scale = out_h / 720.0f;
    cache_clear();
    fonts_open();
}

void ui_quit()
{
    cache_clear();
    fonts_close();
    TTF_Quit();
}

int   ui_width()  { return g_w; }
int   ui_height() { return g_h; }
float ui_scale()  { return g_scale; }

static int S(int v) { return (int)(v * g_scale + 0.5f); }

static TTF_Font* font(int size)
{
    if (size < 1 || size >= kFontCount || !g_fonts[size]) size = 2;
    return g_fonts[size];
}

static const TextEntry* text_entry(const std::string& s, int size, SDL_Color c)
{
    char key_prefix[32];
    snprintf(key_prefix, sizeof(key_prefix), "%d:%02x%02x%02x%02x:", size, c.r, c.g, c.b, c.a);
    std::string key = key_prefix + s;

    auto it = g_cache.find(key);
    if (it != g_cache.end()) return &it->second;

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font(size), s.c_str(), c);
    if (!surf) return nullptr;
    TextEntry e = {SDL_CreateTextureFromSurface(g_r, surf), surf->w, surf->h};
    SDL_FreeSurface(surf);
    if (!e.tex) return nullptr;

    if (g_cache.size() >= kCacheMax)
    {
        auto old = g_cache.find(g_lru.front());
        if (old != g_cache.end())
        {
            SDL_DestroyTexture(old->second.tex);
            g_cache.erase(old);
        }
        g_lru.pop_front();
    }
    g_lru.push_back(key);
    return &(g_cache[key] = e);
}

int ui_text_width(const std::string& s, int size)
{
    int w = 0, h = 0;
    TTF_SizeUTF8(font(size), s.c_str(), &w, &h);
    return (int)(w / g_scale + 0.5f);
}

void ui_text(const std::string& s, int x, int y, int size, SDL_Color c, Align a)
{
    if (s.empty()) return;
    const TextEntry* e = text_entry(s, size, c);
    if (!e) return;

    SDL_Rect dst = {S(x), S(y), e->w, e->h};
    if (a == Align::Center) dst.x -= e->w / 2;
    else if (a == Align::Right) dst.x -= e->w;
    SDL_SetTextureAlphaMod(e->tex, c.a);
    SDL_RenderCopy(g_r, e->tex, nullptr, &dst);
}

void ui_fill(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawBlendMode(g_r, c.a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_Rect rc = {S(x), S(y), S(w), S(h)};
    SDL_RenderFillRect(g_r, &rc);
}

void ui_frame(int x, int y, int w, int h, SDL_Color c)
{
    ui_fill(x, y, w, 2, c);
    ui_fill(x, y + h - 2, w, 2, c);
    ui_fill(x, y, 2, h, c);
    ui_fill(x + w - 2, y, 2, h, c);
}

void ui_clear(SDL_Color c)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, 255);
    SDL_RenderClear(g_r);
}

void ui_toast(const std::string& msg)
{
    g_toast = msg;
    g_toast_until = SDL_GetTicks() + 2000;
}

void ui_draw_toast()
{
    Uint32 now = SDL_GetTicks();
    if (g_toast.empty() || now >= g_toast_until) return;

    int left = (int)(g_toast_until - now);
    Uint8 alpha = left < 300 ? (Uint8)(left * 255 / 300) : 255;
    int w = ui_text_width(g_toast, 2) + 40;
    ui_fill(640 - w / 2, 640, w, 44, {0, 0, 0, (Uint8)(alpha * 3 / 4)});
    SDL_Color c = Col::White;
    c.a = alpha;
    ui_text(g_toast, 640, 653, 2, c, Align::Center);
}

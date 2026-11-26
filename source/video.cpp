// Mario Kart DS NX - placing and drawing the two DS screens
#include "video.h"
#include "core.h"
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static SDL_Renderer* g_r = nullptr;
static SDL_Texture*  g_screen[2] = {};    // 256x192 streaming, 0 = top, 1 = bottom
static SDL_Texture*  g_sharp[2] = {};     // integer-scaled copy for SHARP+SMOOTH
static int           g_sharp_k[2] = {};
static uint64_t      g_uploaded = ~0ull;

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static float size_factor(int size)
{
    switch (size)
    {
        case Size::S90: return 0.9f;
        case Size::S80: return 0.8f;
        case Size::S70: return 0.7f;
        default:        return 1.0f;
    }
}

static SDL_Rect rect(float x, float y, float w, float h)
{
    return {(int)lroundf(x), (int)lroundf(y), (int)lroundf(w), (int)lroundf(h)};
}

ScreenRects video_layout(const Settings& cfg, int W, int H, bool swapped)
{
    ScreenRects out = {};
    const bool integer = cfg.screen_size == Size::Integer;
    const float f = size_factor(cfg.screen_size);
    SDL_Rect a, b;   // a = first/big screen, b = second/small screen

    switch (cfg.layout)
    {
        case Layout::Focus:
        {
            // Big screen plus one a third of its size beside it, bottom
            // aligned. 4:3 + 1/3 of 4:3 is exactly 16:9, so FIT fills the
            // TV: 960x720 + 320x240 at 720p.
            float s = std::min(W / (DS_W * 4.0f / 3.0f), H / (float)DS_H) * f;
            float sb = s, ss = s / 3.0f;
            if (integer)
            {
                sb = std::max(1.0f, floorf(s));
                ss = std::min((W - DS_W * sb) / DS_W, sb);
                if (ss >= 1.0f) ss = floorf(ss);
                ss = std::max(ss, 0.25f);
            }
            float gw = DS_W * (sb + ss), gh = DS_H * sb;
            float x = (W - gw) / 2, y = (H - gh) / 2;
            a = rect(x, y, DS_W * sb, DS_H * sb);
            b = rect(x + DS_W * sb, y + gh - DS_H * ss, DS_W * ss, DS_H * ss);
            break;
        }
        case Layout::SideBySide:
        {
            float s = std::min(W / (DS_W * 2.0f), H / (float)DS_H) * f;
            if (integer) s = std::max(1.0f, floorf(s));
            float x = (W - DS_W * 2 * s) / 2, y = (H - DS_H * s) / 2;
            a = rect(x, y, DS_W * s, DS_H * s);
            b = rect(x + DS_W * s, y, DS_W * s, DS_H * s);
            break;
        }
        case Layout::Stacked:
        {
            float s = std::min(W / (float)DS_W, H / (DS_H * 2.0f)) * f;
            if (integer) s = std::max(1.0f, floorf(s));
            float x = (W - DS_W * s) / 2, y = (H - DS_H * 2 * s) / 2;
            a = rect(x, y, DS_W * s, DS_H * s);
            b = rect(x, y + DS_H * s, DS_W * s, DS_H * s);
            break;
        }
        default:   // Layout::Single
        {
            float s = std::min(W / (float)DS_W, H / (float)DS_H) * f;
            if (integer) s = std::max(1.0f, floorf(s));
            a = rect((W - DS_W * s) / 2, (H - DS_H * s) / 2, DS_W * s, DS_H * s);
            b = a;
            out.top = a;
            out.bottom = b;
            out.show_top = !swapped;
            out.show_bottom = swapped;
            return out;
        }
    }

    out.top = swapped ? b : a;
    out.bottom = swapped ? a : b;
    out.show_top = out.show_bottom = true;
    return out;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
bool video_init(SDL_Renderer* r)
{
    g_r = r;
    for (int i = 0; i < 2; i++)
    {
        g_screen[i] = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB888,
                                        SDL_TEXTUREACCESS_STREAMING, DS_W, DS_H);
        if (!g_screen[i])
        {
            printf("Video: texture failed: %s\n", SDL_GetError());
            return false;
        }
    }
    return true;
}

void video_quit()
{
    for (int i = 0; i < 2; i++)
    {
        if (g_screen[i]) SDL_DestroyTexture(g_screen[i]);
        if (g_sharp[i]) SDL_DestroyTexture(g_sharp[i]);
        g_screen[i] = g_sharp[i] = nullptr;
        g_sharp_k[i] = 0;
    }
}

void video_upload()
{
    uint64_t n = core_frame_count();
    if (n == g_uploaded) return;
    g_uploaded = n;

    const uint32_t* frame = core_frame_acquire();
    for (int i = 0; i < 2; i++)
        SDL_UpdateTexture(g_screen[i], nullptr, frame + i * DS_W * DS_H, DS_W * 4);
    core_frame_release();
}

// SHARP+SMOOTH: blow the screen up with nearest-neighbour to the next whole
// multiple above its on-screen size, then let linear filtering shrink it.
// Pixels stay square and crisp, and only the edges where the scale does not
// divide evenly get blended.
static SDL_Texture* sharp_copy(int i, int dst_w)
{
    int k = std::clamp((dst_w + DS_W - 1) / DS_W, 1, 8);
    if (g_sharp_k[i] != k || !g_sharp[i])
    {
        if (g_sharp[i]) SDL_DestroyTexture(g_sharp[i]);
        g_sharp[i] = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_TARGET, DS_W * k, DS_H * k);
        g_sharp_k[i] = k;
        if (!g_sharp[i]) return nullptr;
        SDL_SetTextureScaleMode(g_sharp[i], SDL_ScaleModeLinear);
    }
    SDL_SetTextureScaleMode(g_screen[i], SDL_ScaleModeNearest);
    SDL_Texture* prev = SDL_GetRenderTarget(g_r);
    SDL_SetRenderTarget(g_r, g_sharp[i]);
    SDL_RenderCopy(g_r, g_screen[i], nullptr, nullptr);
    SDL_SetRenderTarget(g_r, prev);
    return g_sharp[i];
}

static void draw_grid(const SDL_Rect& d)
{
    float px = d.w / (float)DS_W;
    if (px < 3.0f) return;   // too small to show lines between pixels

    int t = std::max(1, (int)(px / 5));
    static std::vector<SDL_Rect> lines;
    lines.clear();
    for (int x = 1; x < DS_W; x++)
        lines.push_back({d.x + (int)(x * px) - t / 2, d.y, t, d.h});
    for (int y = 1; y < DS_H; y++)
        lines.push_back({d.x, d.y + (int)(y * px) - t / 2, d.w, t});

    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_r, 0, 0, 0, 56);
    SDL_RenderFillRects(g_r, lines.data(), (int)lines.size());
}

static void draw_screen(int i, const SDL_Rect& d, const Settings& cfg)
{
    SDL_Texture* src = g_screen[i];
    if (cfg.filter == Filter::SharpSmooth)
    {
        if (SDL_Texture* s = sharp_copy(i, d.w)) src = s;
    }
    else
    {
        SDL_SetTextureScaleMode(src, cfg.filter == Filter::Smooth ? SDL_ScaleModeLinear
                                                                  : SDL_ScaleModeNearest);
    }
    SDL_RenderCopy(g_r, src, nullptr, &d);
    if (cfg.effect == Effect::LcdGrid) draw_grid(d);
}

static void draw_cursor(const SDL_Rect& d, int cx, int cy)
{
    float px = d.w / (float)DS_W;
    int x = d.x + (int)((cx + 0.5f) * px);
    int y = d.y + (int)((cy + 0.5f) * d.h / (float)DS_H);
    int arm = std::max(6, (int)(d.w * 0.035f));
    int t = std::max(2, arm / 4);

    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_r, 0, 0, 0, 255);
    SDL_Rect outline[2] = {{x - arm - 1, y - t / 2 - 1, arm * 2 + 2, t + 2},
                           {x - t / 2 - 1, y - arm - 1, t + 2, arm * 2 + 2}};
    SDL_RenderFillRects(g_r, outline, 2);
    SDL_SetRenderDrawColor(g_r, 255, 255, 255, 255);
    SDL_Rect cross[2] = {{x - arm, y - t / 2, arm * 2, t}, {x - t / 2, y - arm, t, arm * 2}};
    SDL_RenderFillRects(g_r, cross, 2);
}

void video_draw(const Settings& cfg, const ScreenRects& rc, int cursor_x, int cursor_y, float fps)
{
    if (rc.show_top) draw_screen(0, rc.top, cfg);
    if (rc.show_bottom) draw_screen(1, rc.bottom, cfg);
    if (rc.show_bottom && cursor_x >= 0) draw_cursor(rc.bottom, cursor_x, cursor_y);

    if (cfg.fps != Corner::Off)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f FPS", fps);
        bool right = cfg.fps == Corner::TopRight || cfg.fps == Corner::BottomRight;
        bool bottom = cfg.fps == Corner::BottomLeft || cfg.fps == Corner::BottomRight;
        int x = right ? 1268 : 12, y = bottom ? 692 : 12;
        ui_fill(right ? x - ui_text_width(buf, 1) - 6 : x - 6, y - 6,
                ui_text_width(buf, 1) + 12, 24, {0, 0, 0, 160});
        ui_text(buf, x, y, 1, Col::Yellow, right ? Align::Right : Align::Left);
    }
}

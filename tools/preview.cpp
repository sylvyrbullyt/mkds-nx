// Desktop preview of the menus and screen layouts (developer tool, not part of the Switch build).
// Build from the repo root:
//   g++ -std=gnu++17 -Iinclude -Itools/stub -DFONT_PATH='"romfs/fonts/PressStart2P-Regular.ttf"'
//       tools/preview.cpp source/ui.cpp source/video.cpp source/menus.cpp source/config.cpp
//       source/rom.cpp $(sdl2-config --cflags --libs) -lSDL2_ttf -lSDL2_image -o preview
// Run:  SDL_VIDEODRIVER=offscreen ./preview <outdir>
// Writes main-menu.png, settings.png, controls.png, pause-menu.png and one
// layout-*.png per screen layout. The DS screens show a test pattern.
#include "app.h"
#include "core.h"
#include "input.h"
#include "ui.h"
#include "video.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <string>

// ---- stand-ins for the Switch-only parts (input.cpp, core.cpp, main.cpp)
static uint32_t g_frame[DS_W * DS_H * 2];
uint64_t input_down() { return 0; }
uint64_t input_held() { return 0; }
bool input_menu_up() { return false; }
bool input_menu_down() { return false; }
bool input_menu_left() { return false; }
bool input_menu_right() { return false; }
bool input_pause_pressed() { return false; }
void input_game_reset() {}
const char* input_controller_name() { return "PRO CONTROLLER"; }
void input_show_controller_applet() {}
bool core_loaded() { return true; }
void core_wait_idle() {}
bool core_save_state(const char*) { return true; }
bool core_load_state(const char*) { return true; }
void core_reset() {}
void audio_pause(bool) {}
const uint32_t* core_frame_acquire() { return g_frame; }
void core_frame_release() {}
uint64_t core_frame_count() { return 1; }
bool app_start_game(App&) { return true; }
void app_stop_game(App&) {}
void app_apply_resolution(App&) {}
void app_set_playing(App&, bool) {}
std::string app_state_path(int s) { return "/nonexistent/mkds.state" + std::to_string(s); }
void app_draw_game(App& app, bool dim)
{
    ui_clear({0, 0, 0, 255});
    video_draw(app.cfg, video_layout(app.cfg, ui_width(), ui_height(), app.swapped), -1, -1, app.fps);
    if (dim) ui_fill(0, 0, 1280, 720, {0, 0, 0, 150});
}

static std::string g_dir;
static SDL_Renderer* g_r;

static void shot(const char* name)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(g_r, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    std::string path = g_dir + "/" + name;
    IMG_SavePNG(s, path.c_str());
    SDL_FreeSurface(s);
    printf("wrote %s\n", path.c_str());
}

int main(int argc, char** argv)
{
    g_dir = argc > 1 ? argv[1] : ".";
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* w = SDL_CreateWindow("preview", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
    g_r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    if (!ui_init(g_r, 1280, 720) || !video_init(g_r)) { printf("init failed: %s\n", SDL_GetError()); return 1; }

    // Test pattern: cool gradient on top, warm on the bottom, with a grid.
    for (int y = 0; y < DS_H * 2; y++)
        for (int x = 0; x < DS_W; x++)
        {
            bool top = y < DS_H, grid = x % 32 == 0 || y % 32 == 0;
            int r = top ? 20 : 200, g = top ? 60 + y / 2 : 80 + (y - DS_H) / 3, b = top ? 120 + x / 3 : 40;
            if (grid) r = g = b = 230;
            g_frame[y * DS_W + x] = (r << 16) | (g << 8) | b;
        }
    video_upload();

    App app;
    app.renderer = g_r;
    app.rom.status = RomStatus::Ok;
    app.rom.message = "ROM OK  MARIO KART DS (USA)";

    title_draw(app);                                     shot("main-menu.png");
    settings_open(app, Screen::Title); app.set_sel = 1;  settings_draw(app); shot("settings.png");
    app.set_tab = 1; app.set_sel = 0;                    settings_draw(app); shot("controls.png");
    app.screen = Screen::Pause; app.pause_sel = 0;       pause_draw(app);    shot("pause-menu.png");

    const char* names[] = {"layout-focus.png", "layout-side-by-side.png", "layout-stacked.png", "layout-one-screen.png"};
    for (int l = 0; l < 4; l++)
    {
        app.cfg.layout = l;
        ui_clear({0, 0, 0, 255});
        video_draw(app.cfg, video_layout(app.cfg, 1280, 720, false), l == 0 ? 128 : -1, 96, 0);
        shot(names[l]);
    }
    return 0;
}

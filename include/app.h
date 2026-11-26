// Mario Kart DS NX - shared application state
#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

#include "cheats.h"
#include "config.h"
#include "rom.h"

enum class Screen { Title, Settings, Game, Pause };

struct App
{
    SDL_Window*   window = nullptr;
    SDL_Renderer* renderer = nullptr;

    Settings           cfg;
    std::vector<Cheat> cheats;
    RomInfo            rom;
    std::string        crash_note;     // set if the last game did not exit cleanly

    Screen screen = Screen::Title;
    Screen settings_back = Screen::Title;   // where B in Settings returns to
    bool   running = true;
    bool   swapped = false;                 // Swap Screens toggle
    float  fps = 0.0f;

    // menu cursors
    int  title_sel = 0;
    int  set_tab = 0, set_sel = 0, set_scroll = 0;
    bool remap_wait = false;
    int  pause_sel = 0;
    int  slot = 1;
};

// menus.cpp
void title_update(App& app);
void title_draw(App& app);
void settings_open(App& app, Screen back);
void settings_update(App& app);
void settings_draw(App& app);
void pause_open(App& app);
void pause_update(App& app);
void pause_draw(App& app);

// main.cpp
bool app_start_game(App& app);
void app_stop_game(App& app);
void app_apply_resolution(App& app);
void app_set_playing(App& app, bool playing);   // CPU boost on/off
void app_draw_game(App& app, bool dim);
std::string app_state_path(int slot);

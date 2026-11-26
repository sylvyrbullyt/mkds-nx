// Mario Kart DS NX - title screen, settings and pause menu
#include "app.h"
#include "core.h"
#include "input.h"
#include "ui.h"

#include <switch.h>

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

static const char* kFooterChange = "LEFT/RIGHT CHANGE   B SAVE & BACK   L/R TAB";
static const char* kFooterRemap  = "A REMAP   X CLEAR   B SAVE & BACK   L/R TAB";
static const char* kFooterToggle = "A TOGGLE   B SAVE & BACK   L/R TAB";

// Help for the bindings that are not DS buttons.
static const char* kActionHelp[ACT_COUNT] = {
    "", "", "", "", "", "", "", "",
    "Flip which screen is big (or shown).",
    "Press the stylus at the right-stick cursor.",
    "Blow into the microphone (Balloon Battle).",
};

static bool file_exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
enum { TITLE_START, TITLE_SETTINGS, TITLE_QUIT, TITLE_COUNT };
static const char* kTitleItems[TITLE_COUNT] = {"START GAME", "SETTINGS", "QUIT"};

void title_update(App& app)
{
    if (input_menu_up())   app.title_sel = (app.title_sel + TITLE_COUNT - 1) % TITLE_COUNT;
    if (input_menu_down()) app.title_sel = (app.title_sel + 1) % TITLE_COUNT;

    uint64_t down = input_down();
    if (down & HidNpadButton_Plus)
    {
        app.running = false;
        return;
    }
    if (!(down & HidNpadButton_A)) return;

    switch (app.title_sel)
    {
        case TITLE_START:
            if (app.rom.status == RomStatus::Missing || app.rom.status == RomStatus::Error)
            {
                // Look again: the user may have copied the file over since.
                app.rom = rom_check(APP_DIR ROM_FILE);
                if (app.rom.status == RomStatus::Missing || app.rom.status == RomStatus::Error)
                {
                    ui_toast(app.rom.status == RomStatus::Missing ? "PUT " ROM_FILE " IN /switch/mkds-nx/"
                                                                  : app.rom.message);
                    break;
                }
            }
            app_start_game(app);
            break;
        case TITLE_SETTINGS:
            settings_open(app, Screen::Title);
            break;
        case TITLE_QUIT:
            app.running = false;
            break;
    }
}

void title_draw(App& app)
{
    ui_clear(Col::Bg);

    // Checkered stripe: a nod to a finish line, drawn with plain boxes.
    for (int i = 0; i < 1280 / 20; i++)
        for (int j = 0; j < 2; j++)
            if ((i + j) % 2 == 0) ui_fill(i * 20, 196 + j * 20, 20, 20, Col::Panel);

    int tw = ui_text_width("MARIO KART DS", 5) + ui_text_width(" NX", 5);
    int x = 640 - tw / 2;
    ui_text("MARIO KART DS", x + 4, 104, 5, {0, 0, 0, 255});
    ui_text("MARIO KART DS", x, 100, 5, Col::Red);
    ui_text(" NX", x + ui_text_width("MARIO KART DS", 5), 100, 5, Col::Yellow);

    for (int i = 0; i < TITLE_COUNT; i++)
    {
        int y = 300 + i * 64;
        bool sel = i == app.title_sel;
        if (sel) ui_fill(440, y - 14, 400, 52, Col::Panel);
        if (sel) ui_frame(440, y - 14, 400, 52, Col::Yellow);
        ui_text(kTitleItems[i], 640, y, 3, sel ? Col::White : Col::Grey, Align::Center);
    }

    SDL_Color rc = app.rom.status == RomStatus::Ok      ? Col::Green
                 : app.rom.status == RomStatus::Warning ? Col::Yellow
                                                        : Col::Red;
    ui_text(app.rom.message, 640, 560, 1, rc, Align::Center);
    if (!app.crash_note.empty())
        ui_text(app.crash_note, 640, 590, 1, Col::Yellow, Align::Center);

    ui_text("A SELECT   + QUIT   IN GAME: CLICK A STICK TO PAUSE", 640, 676, 1, Col::Dim, Align::Center);
    ui_draw_toast();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
enum { TAB_VIDEO, TAB_CONTROLS, TAB_GAMEPLAY, TAB_COUNT };
static const char* kTabs[TAB_COUNT] = {"VIDEO", "CONTROLS", "GAMEPLAY"};
static const int kVisibleRows = 9;

static int row_count(const App& app)
{
    switch (app.set_tab)
    {
        case TAB_VIDEO:    return kVideoOptionCount;
        case TAB_CONTROLS: return ACT_COUNT + 1;   // + RESET TO DEFAULTS
        default:           return kGameplayOptionCount + std::max<int>(1, (int)app.cheats.size());
    }
}

void settings_open(App& app, Screen back)
{
    app.settings_back = back;
    app.screen = Screen::Settings;
    app.set_sel = 0;
    app.set_scroll = 0;
    app.remap_wait = false;
}

static void settings_close(App& app)
{
    bool ok = config_save(app.cfg, APP_DIR CONFIG_FILE);
    ui_toast(ok ? "SETTINGS SAVED" : "COULD NOT WRITE CONFIG.INI");
    app.screen = app.settings_back;
}

static void change_option(App& app, const OptionDef& def, int delta)
{
    int& v = app.cfg.*(def.field);
    v = (v + delta + def.count) % def.count;
    if (def.field == &Settings::resolution) app_apply_resolution(app);
}

static void remap(App& app, int action, uint64_t button)
{
    uint64_t old = app.cfg.binds[action];
    // Whoever had this button gives it up; if that leaves them with nothing,
    // they take this action's old button (a swap).
    for (int j = 0; j < ACT_COUNT; j++)
    {
        if (j == action || !(app.cfg.binds[j] & button)) continue;
        app.cfg.binds[j] &= ~button;
        if (!app.cfg.binds[j]) app.cfg.binds[j] = old & ~button;
    }
    app.cfg.binds[action] = button;
}

void settings_update(App& app)
{
    uint64_t down = input_down();

    if (app.remap_wait)
    {
        for (int i = 0; i < kSwitchButtonCount; i++)
        {
            if (down & kSwitchButtons[i].bit)
            {
                remap(app, app.set_sel, kSwitchButtons[i].bit);
                app.remap_wait = false;
                break;
            }
        }
        return;
    }

    if (down & (HidNpadButton_L | HidNpadButton_R))
    {
        int d = (down & HidNpadButton_L) ? TAB_COUNT - 1 : 1;
        app.set_tab = (app.set_tab + d) % TAB_COUNT;
        app.set_sel = app.set_scroll = 0;
        return;
    }
    if (down & HidNpadButton_B)
    {
        settings_close(app);
        return;
    }

    int rows = row_count(app);
    if (input_menu_up())   app.set_sel = (app.set_sel + rows - 1) % rows;
    if (input_menu_down()) app.set_sel = (app.set_sel + 1) % rows;
    if (app.set_sel < app.set_scroll) app.set_scroll = app.set_sel;
    if (app.set_sel >= app.set_scroll + kVisibleRows) app.set_scroll = app.set_sel - kVisibleRows + 1;

    int delta = input_menu_left() ? -1 : input_menu_right() ? 1 : 0;

    switch (app.set_tab)
    {
        case TAB_VIDEO:
            if (delta) change_option(app, kVideoOptions[app.set_sel], delta);
            break;

        case TAB_CONTROLS:
            if (app.set_sel == ACT_COUNT)
            {
                if (down & HidNpadButton_A)
                {
                    app.cfg.reset_controls();
                    ui_toast("CONTROLS RESET TO DEFAULTS");
                }
            }
            else if (down & HidNpadButton_A)
            {
                app.remap_wait = true;
            }
            else if (down & HidNpadButton_X)
            {
                app.cfg.binds[app.set_sel] = 0;
            }
            break;

        case TAB_GAMEPLAY:
            if (app.set_sel < kGameplayOptionCount)
            {
                if (delta) change_option(app, kGameplayOptions[app.set_sel], delta);
            }
            else if (!app.cheats.empty() && ((down & HidNpadButton_A) || delta))
            {
                const std::string& name = app.cheats[app.set_sel - kGameplayOptionCount].name;
                app.cfg.set_cheat(name, !app.cfg.cheat_enabled(name));
            }
            break;
    }
}

static void draw_row(int i, bool sel, const std::string& label, const std::string& value, bool arrows)
{
    int y = 168 + i * 46;
    if (sel)
    {
        ui_fill(80, y - 12, 1120, 42, Col::Panel);
        ui_text(">> ", 92, y, 2, Col::Yellow);
    }
    ui_text(label, 140, y, 2, sel ? Col::White : Col::Grey);
    std::string v = (sel && arrows) ? "< " + value + " >" : value;
    ui_text(v, 1180, y, 2, sel ? Col::Yellow : Col::Grey, Align::Right);
}

void settings_draw(App& app)
{
    if (app.settings_back == Screen::Pause && core_loaded())
        app_draw_game(app, true);   // see layout changes behind the menu
    else
        ui_clear(Col::Bg);

    ui_fill(60, 24, 1160, 672, {16, 16, 28, 230});
    ui_text("SETTINGS", 640, 44, 3, Col::White, Align::Center);

    for (int t = 0; t < TAB_COUNT; t++)
    {
        int x = 340 + t * 300, w = 280;
        bool sel = t == app.set_tab;
        ui_fill(x - w / 2, 92, w, 40, sel ? Col::Red : Col::Panel);
        ui_text(kTabs[t], x, 104, 2, sel ? Col::White : Col::Grey, Align::Center);
    }
    ui_text("L", 160, 104, 2, Col::Dim);
    ui_text("R", 1120, 104, 2, Col::Dim, Align::Right);

    std::string help, footer = kFooterChange;
    int rows = row_count(app);
    for (int r = app.set_scroll; r < rows && r < app.set_scroll + kVisibleRows; r++)
    {
        int i = r - app.set_scroll;
        bool sel = r == app.set_sel;

        if (app.set_tab == TAB_VIDEO || (app.set_tab == TAB_GAMEPLAY && r < kGameplayOptionCount))
        {
            const OptionDef& d = app.set_tab == TAB_VIDEO ? kVideoOptions[r] : kGameplayOptions[r];
            const Choice& c = d.choices[app.cfg.*(d.field)];
            draw_row(i, sel, d.label, c.label, true);
            if (sel) help = c.help;
        }
        else if (app.set_tab == TAB_CONTROLS)
        {
            footer = kFooterRemap;
            if (r == ACT_COUNT)
            {
                draw_row(i, sel, "RESET TO DEFAULTS", "", false);
                if (sel) help = "Restore the original button layout.";
                continue;
            }
            std::string v = (sel && app.remap_wait) ? "PRESS..." : buttons_to_string(app.cfg.binds[r]);
            draw_row(i, sel, kActions[r].label, v, false);
            if (sel)
            {
                if (kActions[r].ingame[0])
                    help = std::string("In the game: ") + kActions[r].ingame + ". Press A, then the new button.";
                else if (kActionHelp[r][0])
                    help = std::string(kActionHelp[r]) + " Press A, then the new button.";
                else
                    help = "Press A, then the new button.";
            }
        }
        else if (app.cheats.empty())
        {
            if (sel) help = "Add cheat codes to cheats.txt on your SD card to see them here.";
            draw_row(i, sel, "NO CHEATS", "", false);
        }
        else
        {
            const Cheat& c = app.cheats[r - kGameplayOptionCount];
            std::string name = c.name.size() > 28 ? c.name.substr(0, 27) + "." : c.name;
            draw_row(i, sel, name, app.cfg.cheat_enabled(c.name) ? "ON" : "OFF", true);
            if (sel)
            {
                help = "Cheat from cheats.txt. Applies the next time you start the game.";
                footer = kFooterToggle;
            }
        }
    }

    if (app.set_scroll > 0) ui_text("^", 1200, 148, 1, Col::Dim, Align::Right);
    if (app.set_scroll + kVisibleRows < rows) ui_text("v", 1200, 572, 1, Col::Dim, Align::Right);

    ui_text(help, 640, 604, 1, Col::White, Align::Center);
    ui_text(footer, 640, 660, 1, Col::Dim, Align::Center);
    ui_draw_toast();
}

// ---------------------------------------------------------------------------
// Pause menu
// ---------------------------------------------------------------------------
enum { P_RESUME, P_SAVE, P_LOAD, P_CONTROLLERS, P_SETTINGS, P_RESET, P_QUIT, P_COUNT };
static const char* kPauseItems[P_COUNT] = {"RESUME", "SAVE STATE", "LOAD STATE",
                                           "CONTROLLERS", "SETTINGS", "RESET GAME", "QUIT TO MENU"};

void pause_open(App& app)
{
    app.screen = Screen::Pause;
    app.pause_sel = 0;
    core_wait_idle();   // let the frame in progress finish before silencing
    audio_pause(true);
    app_set_playing(app, false);
}

static void pause_resume(App& app)
{
    app.screen = Screen::Game;
    input_game_reset();
    audio_pause(false);
    app_set_playing(app, true);
}

void pause_update(App& app)
{
    uint64_t down = input_down();
    if (down & HidNpadButton_B || input_pause_pressed())
    {
        pause_resume(app);
        return;
    }

    if (input_menu_up())   app.pause_sel = (app.pause_sel + P_COUNT - 1) % P_COUNT;
    if (input_menu_down()) app.pause_sel = (app.pause_sel + 1) % P_COUNT;

    if (app.pause_sel == P_SAVE || app.pause_sel == P_LOAD)
    {
        if (input_menu_left())  app.slot = app.slot == 1 ? STATE_SLOTS : app.slot - 1;
        if (input_menu_right()) app.slot = app.slot == STATE_SLOTS ? 1 : app.slot + 1;
    }

    if (!(down & HidNpadButton_A)) return;

    char msg[64];
    std::string path = app_state_path(app.slot);
    switch (app.pause_sel)
    {
        case P_RESUME:
            pause_resume(app);
            break;
        case P_SAVE:
            if (core_save_state(path.c_str()))
                snprintf(msg, sizeof(msg), "SAVED TO SLOT %d", app.slot);
            else
                snprintf(msg, sizeof(msg), "SAVE STATE FAILED");
            ui_toast(msg);
            break;
        case P_LOAD:
            if (!file_exists(path))
                snprintf(msg, sizeof(msg), "SLOT %d IS EMPTY", app.slot);
            else if (core_load_state(path.c_str()))
            {
                snprintf(msg, sizeof(msg), "LOADED SLOT %d", app.slot);
                ui_toast(msg);
                pause_resume(app);
                break;
            }
            else
                snprintf(msg, sizeof(msg), "LOAD STATE FAILED");
            ui_toast(msg);
            break;
        case P_CONTROLLERS:
            input_show_controller_applet();
            break;
        case P_SETTINGS:
            settings_open(app, Screen::Pause);
            break;
        case P_RESET:
            core_reset();
            pause_resume(app);
            break;
        case P_QUIT:
            app_stop_game(app);
            app.screen = Screen::Title;
            break;
    }
}

void pause_draw(App& app)
{
    app_draw_game(app, true);

    ui_fill(350, 110, 580, 500, {16, 16, 28, 235});
    ui_frame(350, 110, 580, 500, Col::Red);
    ui_text("PAUSED", 640, 136, 3, Col::White, Align::Center);

    for (int i = 0; i < P_COUNT; i++)
    {
        int y = 200 + i * 52;
        bool sel = i == app.pause_sel;
        if (sel) ui_fill(370, y - 12, 540, 42, Col::Panel);

        std::string label = kPauseItems[i];
        if (i == P_SAVE || i == P_LOAD)
        {
            char buf[32];
            bool empty = !file_exists(app_state_path(app.slot));
            snprintf(buf, sizeof(buf), sel ? "< SLOT %d >" : "SLOT %d", app.slot);
            label += std::string("  ") + buf + (i == P_LOAD && empty ? " EMPTY" : "");
        }
        ui_text(label, 640, y, 2, sel ? Col::Yellow : Col::Grey, Align::Center);
    }

    ui_text(input_controller_name(), 640, 572, 1, Col::Dim, Align::Center);
    ui_text("A SELECT   B RESUME", 640, 660, 1, Col::Dim, Align::Center);
    ui_draw_toast();
}

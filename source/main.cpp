// Mario Kart DS NX
//
// A single-game Nintendo Switch frontend for the melonDS emulator core.
// It boots the user's own Mario Kart DS (USA) dump from
// /switch/mkds-nx/mkds.nds and presents it as a native-feeling port:
// title screen, settings, pause menu, save states, two-screen layouts
// and touch input.
#include <switch.h>
#include <SDL2/SDL.h>

#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"
#include "core.h"
#include "input.h"
#include "perf.h"
#include "ui.h"
#include "video.h"

#define RUNNING_FLAG APP_DIR "running.flag"

// ---------------------------------------------------------------------------
// Applet mode guard. Started from the Album the app only gets a small slice
// of memory, which is not enough for a 32 MB ROM plus the emulator.
// ---------------------------------------------------------------------------
static bool full_memory_or_explain()
{
    AppletType t = appletGetAppletType();
    if (t == AppletType_Application || t == AppletType_SystemApplication)
        return true;

    consoleInit(nullptr);
    printf("\x1b[10;10H" APP_NAME " needs full memory access.");
    printf("\x1b[13;10HIt was started in applet mode (e.g. from the Album).");
    printf("\x1b[15;10HPlease relaunch it by holding R while starting a game,");
    printf("\x1b[16;10Hthen open it from the Homebrew Menu.");
    printf("\x1b[20;10HPress any button to exit.");
    consoleUpdate(nullptr);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop())
    {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad)) break;
        svcSleepThread(16000000);
    }
    consoleExit(nullptr);
    return false;
}

// ---------------------------------------------------------------------------
// Helpers used by the menus
// ---------------------------------------------------------------------------
std::string app_state_path(int slot)
{
    return std::string(APP_DIR STATE_FILE) + std::to_string(slot);
}

static void output_size_for(const Settings& cfg, int& w, int& h)
{
    bool docked = appletGetOperationMode() == AppletOperationMode_Console;
    bool big = cfg.resolution == Res::P1080 || (cfg.resolution == Res::Auto && docked);
    w = big ? 1920 : 1280;
    h = big ? 1080 : 720;
}

void app_apply_resolution(App& app)
{
    int w, h;
    output_size_for(app.cfg, w, h);
    int cw = 0, ch = 0;
    SDL_GetRendererOutputSize(app.renderer, &cw, &ch);
    if (cw != w || ch != h)
    {
        SDL_SetWindowSize(app.window, w, h);
        SDL_GetRendererOutputSize(app.renderer, &cw, &ch);
        printf("Video: output %dx%d\n", cw, ch);
    }
    ui_resize(cw, ch);   // whatever we actually got
}

bool app_start_game(App& app)
{
    if (app.rom.data.empty())
        app.rom = rom_check(APP_DIR ROM_FILE);
    if (app.rom.data.empty())
    {
        ui_toast(app.rom.message);
        return false;
    }

    // A flag file that only a clean exit removes: if it is still there next
    // launch, the game crashed or the console was switched off mid-game.
    if (FILE* f = fopen(RUNNING_FLAG, "w")) fclose(f);

    app.cheats = cheats_load(APP_DIR CHEATS_FILE);
    if (!core_load(app.cfg, app.rom.data))
    {
        remove(RUNNING_FLAG);
        ui_toast("THE EMULATOR COULD NOT LOAD " ROM_FILE);
        return false;
    }
    core_set_cheats(app.cheats, app.cfg);

    // The core keeps its own copy of the cartridge.
    std::vector<uint8_t>().swap(app.rom.data);

    app.crash_note.clear();
    app.swapped = false;
    app.screen = Screen::Game;
    input_game_reset();
    app_set_playing(app, true);
    return true;
}

void app_stop_game(App& app)
{
    core_unload();
    app_set_playing(app, false);
    remove(RUNNING_FLAG);
    // Re-check so START GAME has the ROM ready again.
    app.rom = rom_check(APP_DIR ROM_FILE);
}

// The CPU boost is only on while the game is actually running: not on the
// title screen, in the pause menu or while the HOME menu is up.
void app_set_playing(App& app, bool playing)
{
    if (playing && app.cfg.cpu_clock == Clock::Boost)
        perf_set_cpu(1785);
    else
        perf_restore();
}

void app_draw_game(App& app, bool dim)
{
    ui_clear({0, 0, 0, 255});
    ScreenRects rc = video_layout(app.cfg, ui_width(), ui_height(), app.swapped);
    video_draw(app.cfg, rc, -1, -1, app.fps);
    if (dim) ui_fill(0, 0, 1280, 720, {0, 0, 0, 150});
}

// ---------------------------------------------------------------------------
// One frame of gameplay
// ---------------------------------------------------------------------------
static void game_frame(App& app)
{
    if (input_pause_pressed() || appletGetFocusState() != AppletFocusState_InFocus)
    {
        pause_open(app);
        return;
    }

    ScreenRects rc = video_layout(app.cfg, ui_width(), ui_height(), app.swapped);
    GameInput gi = input_game(app.cfg, rc);
    if (gi.swap_pressed)
    {
        app.swapped = !app.swapped;
        rc = video_layout(app.cfg, ui_width(), ui_height(), app.swapped);
    }

    int runs = 1;
    if (app.cfg.fast_forward != FastFwd::Off && input_fast_forward_held())
        runs = app.cfg.fast_forward + 1;   // X2 -> 2 frames, X3 -> 3, X4 -> 4

    // Hand this refresh's input to the emulation thread. If it is still busy
    // with the previous frame, skip: the latest finished frame is shown again
    // and the emulator picks up fresh input as soon as it is free.
    core_submit(gi.core, runs);

    video_upload();
    ui_clear({0, 0, 0, 255});
    video_draw(app.cfg, rc, gi.cursor_x, gi.cursor_y, app.fps);
    ui_draw_toast();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int, char**)
{
    if (!full_memory_or_explain()) return 0;

    romfsInit();
    mkdir("sdmc:/switch", 0777);
    mkdir(APP_DIR, 0777);
    // Log to the SD card as well as nxlink, which helps with bug reports.
    freopen(APP_DIR "log.txt", "w", stdout);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf(APP_NAME " starting\n");

    App app;
    if (!config_load(app.cfg, APP_DIR CONFIG_FILE))
        config_save(app.cfg, APP_DIR CONFIG_FILE);   // write a commented default
    app.cheats = cheats_load(APP_DIR CHEATS_FILE);

    struct stat st;
    if (stat(RUNNING_FLAG, &st) == 0)
    {
        remove(RUNNING_FLAG);
        app.crash_note = app.cfg.cpu == Cpu::Jit
            ? "LAST GAME DID NOT CLOSE CLEANLY. IF IT CRASHED, TRY CPU MODE: SAFE"
            : "LAST GAME DID NOT CLOSE CLEANLY.";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    int w, h;
    output_size_for(app.cfg, w, h);
    app.window = SDL_CreateWindow(APP_NAME, 0, 0, w, h, 0);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app.window || !app.renderer)
    {
        printf("SDL window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GetRendererOutputSize(app.renderer, &w, &h);
    if (!ui_init(app.renderer, w, h) || !video_init(app.renderer))
        return 1;
    input_init();

    app.rom = rom_check(APP_DIR ROM_FILE);

    Uint32 fps_t0 = SDL_GetTicks();
    uint64_t fps_f0 = core_frame_count();
    AppletOperationMode mode = appletGetOperationMode();

    while (app.running && appletMainLoop())
    {
        // Docking / undocking changes the AUTO resolution.
        AppletOperationMode m = appletGetOperationMode();
        if (m != mode)
        {
            mode = m;
            app_apply_resolution(app);
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT) app.running = false;

        input_update();

        switch (app.screen)
        {
            case Screen::Title:    title_update(app);    break;
            case Screen::Settings: settings_update(app); break;
            case Screen::Pause:    pause_update(app);    break;
            case Screen::Game:     break;
        }

        // Drawing (game_frame both runs and draws the game).
        switch (app.screen)
        {
            case Screen::Title:    title_draw(app);    break;
            case Screen::Settings: settings_draw(app); break;
            case Screen::Pause:    pause_draw(app);    break;
            case Screen::Game:     game_frame(app);    break;
        }
        SDL_RenderPresent(app.renderer);

        Uint32 now = SDL_GetTicks();
        if (now - fps_t0 >= 1000)
        {
            uint64_t f = core_frame_count();
            app.fps = (f - fps_f0) * 1000.0f / (now - fps_t0);
            fps_t0 = now;
            fps_f0 = f;
        }
    }

    if (core_loaded()) app_stop_game(app);
    perf_restore();
    video_quit();
    ui_quit();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    romfsExit();
    printf(APP_NAME " exiting\n");
    fclose(stdout);
    return 0;
}

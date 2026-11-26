// Mario Kart DS NX - controllers, touch screen and the stylus cursor
#include "input.h"
#include "ui.h"

#include <switch.h>
#include <libretro.h>

#include <algorithm>
#include <cmath>

static PadState g_pad;
static uint64_t g_held = 0, g_down = 0;
static int      g_stick_lx = 0, g_stick_ly = 0, g_stick_rx = 0, g_stick_ry = 0;

// key repeat for menus
static int      g_repeat_dir = -1;
static uint32_t g_repeat_frames = 0;
static bool     g_dir_fire[4] = {};

// stylus cursor
static float    g_cur_x = DS_W / 2, g_cur_y = DS_H / 2;
static int      g_cur_idle = 1000;   // frames since the cursor last moved/pressed
static bool     g_screen_touch = false;

static const int kStickDead   = 8000;
static const int kCursorHide  = 180;   // ~3 s
static const int kRepeatDelay = 18;
static const int kRepeatRate  = 5;

void input_init()
{
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
    hidInitializeTouchScreen();
}

void input_update()
{
    padUpdate(&g_pad);
    g_held = padGetButtons(&g_pad);
    g_down = padGetButtonsDown(&g_pad);

    HidAnalogStickState l = padGetStickPos(&g_pad, 0);
    HidAnalogStickState r = padGetStickPos(&g_pad, 1);
    g_stick_lx = l.x; g_stick_ly = l.y;
    g_stick_rx = r.x; g_stick_ry = r.y;

    // Menu directions: d-pad or left stick, first press then auto-repeat.
    int dir = -1;
    if (g_held & (HidNpadButton_Up | HidNpadButton_StickLUp))         dir = 0;
    else if (g_held & (HidNpadButton_Down | HidNpadButton_StickLDown)) dir = 1;
    else if (g_held & (HidNpadButton_Left | HidNpadButton_StickLLeft)) dir = 2;
    else if (g_held & (HidNpadButton_Right | HidNpadButton_StickLRight)) dir = 3;

    for (bool& f : g_dir_fire) f = false;
    if (dir < 0)
    {
        g_repeat_dir = -1;
    }
    else if (dir != g_repeat_dir)
    {
        g_repeat_dir = dir;
        g_repeat_frames = 0;
        g_dir_fire[dir] = true;
    }
    else
    {
        g_repeat_frames++;
        if (g_repeat_frames >= kRepeatDelay && (g_repeat_frames - kRepeatDelay) % kRepeatRate == 0)
            g_dir_fire[dir] = true;
    }
}

uint64_t input_held() { return g_held; }
uint64_t input_down() { return g_down; }

bool input_menu_up()    { return g_dir_fire[0]; }
bool input_menu_down()  { return g_dir_fire[1]; }
bool input_menu_left()  { return g_dir_fire[2]; }
bool input_menu_right() { return g_dir_fire[3]; }

bool input_pause_pressed()
{
    return g_down & (HidNpadButton_StickL | HidNpadButton_StickR);
}

bool input_fast_forward_held()
{
    return g_stick_rx > 24000 && std::abs(g_stick_ry) < 16000;
}

void input_game_reset()
{
    g_screen_touch = false;
    g_cur_idle = kCursorHide;
}

// Maps a point in output pixels onto the bottom screen, if it is on it.
static bool to_ds(const ScreenRects& rc, float x, float y, int& dx, int& dy)
{
    if (!rc.show_bottom) return false;
    const SDL_Rect& b = rc.bottom;
    if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) return false;
    dx = std::clamp((int)((x - b.x) * DS_W / b.w), 0, DS_W - 1);
    dy = std::clamp((int)((y - b.y) * DS_H / b.h), 0, DS_H - 1);
    return true;
}

GameInput input_game(const Settings& cfg, const ScreenRects& rc)
{
    static const int kJoypad[8] = {
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_Y,
        RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT,
    };

    GameInput out = {};
    uint16_t pad = 0;

    // DS buttons from the bindings.
    for (int a = ACT_DS_A; a <= ACT_DS_SELECT; a++)
        if (g_held & cfg.binds[a])
            pad |= 1 << kJoypad[a];

    // The d-pad and the left stick both steer.
    if (g_held & (HidNpadButton_Up | HidNpadButton_StickLUp))       pad |= 1 << RETRO_DEVICE_ID_JOYPAD_UP;
    if (g_held & (HidNpadButton_Down | HidNpadButton_StickLDown))   pad |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN;
    if (g_held & (HidNpadButton_Left | HidNpadButton_StickLLeft))   pad |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT;
    if (g_held & (HidNpadButton_Right | HidNpadButton_StickLRight)) pad |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT;

    // The core treats L2 as "make microphone noise" (blow).
    if (g_held & cfg.binds[ACT_MIC]) pad |= 1 << RETRO_DEVICE_ID_JOYPAD_L2;

    out.swap_pressed = (g_down & cfg.binds[ACT_SWAP]) != 0;

    // 1) Real touch screen (handheld): touch exactly where the finger is.
    HidTouchScreenState ts = {};
    bool finger = hidGetTouchScreenStates(&ts, 1) && ts.count > 0;
    int dx, dy;
    if (finger)
    {
        // The panel is always 1280x720; scale to the output size.
        float x = ts.touches[0].x * ui_width() / 1280.0f;
        float y = ts.touches[0].y * ui_height() / 720.0f;
        if (to_ds(rc, x, y, dx, dy))
        {
            out.core.touching = true;
            out.core.touch_x = dx;
            out.core.touch_y = dy;
            g_screen_touch = true;
            g_cur_idle = kCursorHide;   // hide the stick cursor while using a finger
        }
    }
    else
    {
        g_screen_touch = false;
    }

    // 2) Stylus cursor: right stick moves it (unless it is held right for
    //    fast-forward), the TOUCH button presses.
    if (!out.core.touching)
    {
        bool ff = cfg.fast_forward != FastFwd::Off && input_fast_forward_held();
        float rx = std::abs(g_stick_rx) > kStickDead ? g_stick_rx / 32767.0f : 0.0f;
        float ry = std::abs(g_stick_ry) > kStickDead ? g_stick_ry / 32767.0f : 0.0f;
        if (!ff && (rx != 0.0f || ry != 0.0f))
        {
            // Squared response: slow near the centre for precise taps.
            const float speed = 4.5f;
            g_cur_x = std::clamp(g_cur_x + rx * std::abs(rx) * speed, 0.0f, DS_W - 1.0f);
            g_cur_y = std::clamp(g_cur_y - ry * std::abs(ry) * speed, 0.0f, DS_H - 1.0f);
            g_cur_idle = 0;
        }

        bool press = (g_held & cfg.binds[ACT_TOUCH]) != 0;
        if (press)
        {
            out.core.touching = true;
            out.core.touch_x = (int)g_cur_x;
            out.core.touch_y = (int)g_cur_y;
            g_cur_idle = 0;
        }
        else if (g_cur_idle < kCursorHide)
        {
            g_cur_idle++;
        }
    }

    out.cursor_x = g_cur_idle < kCursorHide ? (int)g_cur_x : -1;
    out.cursor_y = (int)g_cur_y;
    out.core.joypad = pad;
    return out;
}

const char* input_controller_name()
{
    HidNpadIdType id = padIsHandheld(&g_pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1;
    u32 style = hidGetNpadStyleSet(id);
    if (style & HidNpadStyleTag_NpadHandheld)  return "HANDHELD";
    if (style & HidNpadStyleTag_NpadFullKey)   return "PRO CONTROLLER";
    if (style & HidNpadStyleTag_NpadJoyDual)   return "JOY-CON PAIR";
    if (style & HidNpadStyleTag_NpadJoyLeft)   return "JOY-CON (L)";
    if (style & HidNpadStyleTag_NpadJoyRight)  return "JOY-CON (R)";
    return "CONTROLLER";
}

void input_show_controller_applet()
{
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = 0;
    arg.hdr.player_count_max = 1;
    arg.hdr.enable_single_mode = true;
    HidLaControllerSupportResultInfo info;
    hidLaShowControllerSupport(&info, &arg);
}

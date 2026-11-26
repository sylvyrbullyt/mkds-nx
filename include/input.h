// Mario Kart DS NX - controllers, touch screen and the stylus cursor
#pragma once

#include <cstdint>

#include "config.h"
#include "core.h"
#include "video.h"

void input_init();

// Call once per displayed frame.
void input_update();

uint64_t input_held();   // HidNpadButton bits (both Joy-Con, handheld and player 1)
uint64_t input_down();   // pressed this frame

// Menu helpers: d-pad or left stick, with key repeat.
bool input_menu_up();
bool input_menu_down();
bool input_menu_left();
bool input_menu_right();

// True on the frame either stick is clicked (the in-game pause).
bool input_pause_pressed();
// Right stick pushed right (fast-forward).
bool input_fast_forward_held();

// Builds this frame's DS input: mapped buttons, then touch from the Switch
// touch screen (handheld) or the right-stick cursor + TOUCH button (docked).
// Also reports the Swap Screens press so the caller can flip the layout.
struct GameInput
{
    CoreInput core;
    bool      swap_pressed;
    int       cursor_x;   // -1 when the cursor is hidden
    int       cursor_y;
};
GameInput input_game(const Settings& cfg, const ScreenRects& rc);
void      input_game_reset();   // forget held touches / cursor (after menus)

// Short name of the active controller ("HANDHELD", "PRO CONTROLLER", ...).
const char* input_controller_name();

// Opens the system controller applet (pairing / changing grip).
void input_show_controller_applet();

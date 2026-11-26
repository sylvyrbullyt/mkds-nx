// Mario Kart DS NX - melonDS libretro core bridge
//
// The core is linked in statically. It is always asked for the plain
// "Top/Bottom" layout (256x384, no gap) and the frontend cuts that frame
// into the two screens and places them itself (see video.cpp), so every
// layout, swap and touch mapping lives on our side.
//
// Threads: emulation runs on its own thread on CPU core 1, melonDS's 3D
// renderer on core 2, and the menus/presentation on core 0. The main loop
// hands the emulator one frame's input per screen refresh and never waits
// for it, so a slow frame costs that frame only instead of halving the
// frame rate (60 -> 30) through vsync.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cheats.h"
#include "config.h"

#define DS_W 256
#define DS_H 192

// What the frontend feeds the core each frame.
struct CoreInput
{
    uint16_t joypad = 0;      // RETRO_DEVICE_ID_JOYPAD_* bits
    bool     touching = false;
    int      touch_x = 0;     // DS bottom-screen pixels, 0..255
    int      touch_y = 0;     // 0..191
};

bool core_load(const Settings& cfg, const std::vector<uint8_t>& rom);
void core_unload();
bool core_loaded();

// Asks the emulation thread for `runs` frames (more than 1 = fast-forward;
// only the last one's sound plays). Returns false without blocking if the
// previous request is still running; the caller just shows the last frame.
bool core_submit(const CoreInput& in, int runs);
// Blocks until the emulation thread is idle (menus, states, reset).
void core_wait_idle();

void core_reset();
void core_set_cheats(const std::vector<Cheat>& all, const Settings& cfg);

// Latest finished frame: DS_W x (DS_H*2), XRGB8888, top screen first.
// Hold it between acquire and release (the emulator keeps drawing into the
// other buffer meanwhile).
const uint32_t* core_frame_acquire();
void            core_frame_release();
uint64_t        core_frame_count();   // bumps when a new frame is finished

bool core_save_state(const char* path);
bool core_load_state(const char* path);

void audio_pause(bool paused);

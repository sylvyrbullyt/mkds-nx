// Mario Kart DS NX - melonDS libretro core bridge
#include "core.h"

#include <switch.h>
#include <SDL2/SDL.h>
#include <libretro.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool              g_loaded = false;
static CoreInput         g_in;                    // input for the frame being run
static bool              g_audio_on = true;

// Double-buffered output: the core draws into g_buf[g_back] while the main
// thread uploads g_buf[g_front]. Swapped (under g_frame_lock) when a
// request finishes.
static uint32_t              g_buf[2][DS_W * DS_H * 2];
static int                   g_front = 0, g_back = 1;
static bool                  g_back_dirty = false;
static std::mutex            g_frame_lock;
static std::atomic<uint64_t> g_frames{0};

// Emulation thread
static Thread                  g_thread;
static bool                    g_thread_up = false;
static std::mutex              g_job_lock;
static std::condition_variable g_job_cv;
static bool                    g_job_pending = false;   // a request is queued or running
static bool                    g_job_quit = false;
static CoreInput               g_job_in;
static int                     g_job_runs = 1;

static SDL_AudioDeviceID g_audio = 0;
static uint32_t          g_audio_frame_bytes = 0;   // one video frame of sound

static std::map<std::string, std::string> g_vars;
static std::vector<std::string>           g_cheat_codes;

// Core options we hand to melonDS. Anything the core asks for that is not in
// this table gets "not set" and the core keeps its own default.
static void build_vars(const Settings& cfg)
{
    static const char* kLang[] = {"English", "French", "Spanish", "German", "Italian", "Japanese"};

    g_vars.clear();
    g_vars["melonds_console_mode"]      = "DS";
    g_vars["melonds_boot_directly"]     = "enabled";
    g_vars["melonds_use_fw_settings"]   = "disabled";
    g_vars["melonds_language"]          = kLang[cfg.language];
    g_vars["melonds_randomize_mac_address"] = "disabled";
    g_vars["melonds_dsi_sdcard"]        = "disabled";
    g_vars["melonds_threaded_renderer"] = "enabled";
    g_vars["melonds_opengl_renderer"]   = "disabled";
    g_vars["melonds_opengl_resolution"] = "1x native (256x192)";
    g_vars["melonds_opengl_better_polygons"] = "disabled";
    g_vars["melonds_opengl_filtering"]  = "nearest";
    g_vars["melonds_mic_input"]         = "Blow Noise";
    g_vars["melonds_audio_bitrate"]     = "Automatic";
    g_vars["melonds_audio_interpolation"] = "None";
    g_vars["melonds_touch_mode"]        = "Touch";
    g_vars["melonds_swapscreen_mode"]   = "Toggle";
    g_vars["melonds_screen_layout"]     = "Top/Bottom";
    g_vars["melonds_screen_gap"]        = "0";
    g_vars["melonds_hybrid_small_screen"] = "Bottom";
    g_vars["melonds_hybrid_ratio"]      = "2";
    g_vars["melonds_jit_enable"]        = cfg.cpu == Cpu::Jit ? "enabled" : "disabled";
    g_vars["melonds_jit_block_size"]    = "32";
    g_vars["melonds_jit_branch_optimisations"]  = "enabled";
    g_vars["melonds_jit_literal_optimisations"] = "enabled";
    g_vars["melonds_jit_fast_memory"]   = "disabled";
}

// ---------------------------------------------------------------------------
// libretro callbacks
// ---------------------------------------------------------------------------
static void core_log(enum retro_log_level level, const char* fmt, ...)
{
    static const char* kLevel[] = {"debug", "info", "warn", "error"};
    if (level == RETRO_LOG_DEBUG) return;
    printf("[core %s] ", kLevel[level & 3]);
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
}

static bool env_cb(unsigned cmd, void* data)
{
    switch (cmd)
    {
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:   // bios7/bios9/firmware (optional)
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:     // mkds.sav
            *(const char**)data = APP_DIR_CORE;
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            retro_variable* var = (retro_variable*)data;
            auto it = g_vars.find(var->key);
            var->value = it == g_vars.end() ? nullptr : it->second.c_str();
            return var->value != nullptr;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *(bool*)data = false;
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return *(const retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            ((retro_log_callback*)data)->log = core_log;
            return true;

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            return true;

        default:
            return false;   // options are ours, so no SET_VARIABLES / v2 etc.
    }
}

static void video_cb(const void* data, unsigned w, unsigned h, size_t pitch)
{
    if (!data) return;   // duplicate frame
    if (w != DS_W || h != DS_H * 2)
    {
        static bool warned = false;
        if (!warned) printf("Video: unexpected frame %ux%u\n", w, h);
        warned = true;
        return;
    }
    // Only the back buffer is touched here; the main thread never reads it.
    const uint8_t* src = (const uint8_t*)data;
    uint32_t* dst = g_buf[g_back];
    for (unsigned y = 0; y < h; y++)
        memcpy(&dst[y * DS_W], src + y * pitch, DS_W * 4);
    g_back_dirty = true;
}

static void audio_sample_cb(int16_t, int16_t) {}

static size_t audio_batch_cb(const int16_t* data, size_t frames)
{
    if (!g_audio || !g_audio_on) return frames;
    // Keep at most ~4 video frames of sound queued: if emulation runs ahead
    // (fast-forward, or vsync slightly faster than the DS) drop the excess
    // instead of drifting into lag.
    if (SDL_GetQueuedAudioSize(g_audio) < g_audio_frame_bytes * 4)
        SDL_QueueAudio(g_audio, data, (Uint32)(frames * 4));
    return frames;
}

static void input_poll_cb() {}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (port != 0) return 0;

    if (device == RETRO_DEVICE_JOYPAD)
    {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)g_in.joypad;
        return id < 16 ? (g_in.joypad >> id) & 1 : 0;
    }

    if (device == RETRO_DEVICE_POINTER)
    {
        // The core maps pointer -0x7FFF..0x7FFF onto its 256x384 frame; the
        // bottom screen is the lower half.
        switch (id)
        {
            case RETRO_DEVICE_ID_POINTER_PRESSED:
                return g_in.touching;
            case RETRO_DEVICE_ID_POINTER_X:
                return (int16_t)(((g_in.touch_x * 2 + 1) * 0x10000) / (DS_W * 2) - 0x8000);
            case RETRO_DEVICE_ID_POINTER_Y:
                return (int16_t)((((DS_H + g_in.touch_y) * 2 + 1) * 0x10000) / (DS_H * 4) - 0x8000);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Audio device
// ---------------------------------------------------------------------------
static void audio_open(double core_rate, double core_fps)
{
    if (g_audio) return;

    // The screen refreshes at 60 Hz and the DS at ~59.83 Hz. Running one DS
    // frame per vsync is 0.3% fast, so ask for 0.3% more samples per second
    // to keep sound and picture in step without stretching either.
    int rate = (int)lround(core_rate * 60.0 / core_fps);
    printf("Audio: %d Hz requested (core %.0f Hz @ %.4f fps)\n", rate, core_rate, core_fps);

    SDL_AudioSpec want = {}, have = {};
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!g_audio)
    {
        printf("Audio: open failed: %s\n", SDL_GetError());
        return;
    }
    g_audio_frame_bytes = (uint32_t)(rate / 60) * 4;
    SDL_PauseAudioDevice(g_audio, 0);
}

void audio_pause(bool paused)
{
    if (!g_audio) return;
    SDL_PauseAudioDevice(g_audio, paused ? 1 : 0);
    if (paused) SDL_ClearQueuedAudio(g_audio);
}

// ---------------------------------------------------------------------------
// Emulation thread
// ---------------------------------------------------------------------------
static void run_one_frame(bool audio)
{
    g_audio_on = audio;
    // The core applies an Action Replay code once per call, so enabled
    // cheats are re-applied every frame, the way a real AR cart does.
    for (size_t i = 0; i < g_cheat_codes.size(); i++)
        retro_cheat_set((unsigned)i, true, g_cheat_codes[i].c_str());
    retro_run();
}

static void emu_thread(void*)
{
    for (;;)
    {
        CoreInput in;
        int runs;
        {
            std::unique_lock<std::mutex> lk(g_job_lock);
            g_job_cv.wait(lk, [] { return g_job_quit || g_job_pending; });
            if (g_job_quit) return;
            in = g_job_in;
            runs = g_job_runs;
        }

        g_in = in;
        for (int i = 0; i < runs; i++)
            run_one_frame(i == runs - 1);   // only the last frame's sound plays

        if (g_back_dirty)
        {
            std::lock_guard<std::mutex> fl(g_frame_lock);
            std::swap(g_front, g_back);
            g_back_dirty = false;
            g_frames++;
        }

        {
            std::lock_guard<std::mutex> lk(g_job_lock);
            g_job_pending = false;
        }
        g_job_cv.notify_all();
    }
}

static bool thread_start()
{
    g_job_quit = false;
    g_job_pending = false;
    // Core 1, same priority as the main thread. 4 MB of stack: melonDS keeps
    // some large structures on the stack.
    Result rc = threadCreate(&g_thread, emu_thread, nullptr, nullptr, 4 * 1024 * 1024, 0x2C, 1);
    if (R_SUCCEEDED(rc)) rc = threadStart(&g_thread);
    if (R_FAILED(rc))
    {
        printf("Emu thread: start failed (0x%x)\n", rc);
        return false;
    }
    g_thread_up = true;
    return true;
}

static void thread_stop()
{
    if (!g_thread_up) return;
    {
        std::lock_guard<std::mutex> lk(g_job_lock);
        g_job_quit = true;
    }
    g_job_cv.notify_all();
    threadWaitForExit(&g_thread);
    threadClose(&g_thread);
    g_thread_up = false;
}

bool core_submit(const CoreInput& in, int runs)
{
    if (!g_loaded) return false;
    {
        std::lock_guard<std::mutex> lk(g_job_lock);
        if (g_job_pending) return false;
        g_job_in = in;
        g_job_runs = runs < 1 ? 1 : runs;
        g_job_pending = true;
    }
    g_job_cv.notify_all();
    return true;
}

void core_wait_idle()
{
    if (!g_thread_up) return;
    std::unique_lock<std::mutex> lk(g_job_lock);
    g_job_cv.wait(lk, [] { return !g_job_pending; });
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
bool core_load(const Settings& cfg, const std::vector<uint8_t>& rom)
{
    if (g_loaded) core_unload();

    build_vars(cfg);
    memset(g_buf, 0, sizeof(g_buf));
    g_front = 0;
    g_back = 1;
    g_back_dirty = false;
    g_in = CoreInput();

    retro_set_environment(env_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);
    retro_init();

    retro_game_info game = {};
    game.path = APP_DIR ROM_FILE;   // the core names mkds.sav after this
    game.data = rom.data();
    game.size = rom.size();
    if (!retro_load_game(&game))
    {
        printf("The emulator could not load " ROM_FILE "\n");
        retro_deinit();
        return false;
    }

    retro_system_av_info av = {};
    retro_get_system_av_info(&av);
    printf("Core: %ux%u, %.4f fps, %.0f Hz\n", av.geometry.base_width,
           av.geometry.base_height, av.timing.fps, av.timing.sample_rate);
    audio_open(av.timing.sample_rate, av.timing.fps);

    if (!thread_start())
    {
        retro_unload_game();
        retro_deinit();
        return false;
    }
    audio_pause(false);

    g_loaded = true;
    return true;
}

void core_unload()
{
    if (!g_loaded) return;
    thread_stop();         // finishes the frame in progress first
    audio_pause(true);
    retro_unload_game();   // flushes mkds.sav
    retro_deinit();
    g_loaded = false;
}

bool core_loaded() { return g_loaded; }

void core_reset()
{
    if (!g_loaded) return;
    core_wait_idle();
    retro_reset();
}

void core_set_cheats(const std::vector<Cheat>& all, const Settings& cfg)
{
    core_wait_idle();
    g_cheat_codes.clear();
    for (const Cheat& c : all)
    {
        if (!cfg.cheat_enabled(c.name)) continue;
        std::string code = c.code;
        // retro_cheat_set copies 128 bytes of the string as the cheat name,
        // so give it at least that much to read.
        if (code.size() < 128) code.resize(128, '\0');
        g_cheat_codes.push_back(code);
        printf("Cheat on: %s = %s\n", c.name.c_str(), c.code.c_str());
    }
}

const uint32_t* core_frame_acquire()
{
    g_frame_lock.lock();
    return g_buf[g_front];
}

void core_frame_release() { g_frame_lock.unlock(); }

uint64_t core_frame_count() { return g_frames.load(); }

bool core_save_state(const char* path)
{
    if (!g_loaded) return false;
    core_wait_idle();
    size_t size = retro_serialize_size();
    if (!size) return false;
    std::vector<uint8_t> buf(size);
    if (!retro_serialize(buf.data(), size)) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(buf.data(), 1, size, f) == size;
    ok = (fclose(f) == 0) && ok;
    return ok;
}

bool core_load_state(const char* path)
{
    if (!g_loaded) return false;
    core_wait_idle();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return false; }

    std::vector<uint8_t> buf((size_t)size);
    bool ok = fread(buf.data(), 1, buf.size(), f) == buf.size();
    fclose(f);
    return ok && retro_unserialize(buf.data(), buf.size());
}

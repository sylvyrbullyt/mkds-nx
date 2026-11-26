// Mario Kart DS NX - settings (config.ini)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define APP_NAME     "Mario Kart DS NX"
#define APP_DIR      "sdmc:/switch/mkds-nx/"
#define APP_DIR_CORE "sdmc:/switch/mkds-nx"   // core joins paths with '/'
#define ROM_FILE     "mkds.nds"
#define CONFIG_FILE  "config.ini"
#define CHEATS_FILE  "cheats.txt"
#define STATE_FILE   "mkds.state"            // + slot number
#define STATE_SLOTS  4

// ---------------------------------------------------------------------------
// Multiple-choice options. Every option in the Settings menu is one of these:
// a label, its key in config.ini, and a list of choices (label, ini value,
// one line of help). The menu and the ini reader/writer both walk this table,
// so adding an option means adding one entry here and one field in Settings.
// ---------------------------------------------------------------------------
struct Choice
{
    const char* label;
    const char* value;
    const char* help;
};

struct Settings;

struct OptionDef
{
    const char*   label;
    const char*   key;
    const Choice* choices;
    int           count;
    int Settings::*field;
};

namespace Res      { enum { Auto, P720, P1080 }; }
namespace Layout   { enum { Focus, SideBySide, Stacked, Single }; }
namespace Size     { enum { Fit, Integer, S90, S80, S70 }; }
namespace Filter   { enum { Sharp, Smooth, SharpSmooth }; }
namespace Effect   { enum { None, LcdGrid }; }
namespace Corner   { enum { Off, TopLeft, TopRight, BottomLeft, BottomRight }; }
namespace FastFwd  { enum { Off, X2, X3, X4 }; }
namespace Lang     { enum { English, French, Spanish, German, Italian, Japanese }; }
namespace Cpu      { enum { Jit, Interpreter }; }
namespace Clock    { enum { Boost, Stock }; }

// Things a Switch button can be bound to.
enum Action
{
    ACT_DS_A, ACT_DS_B, ACT_DS_X, ACT_DS_Y,
    ACT_DS_L, ACT_DS_R, ACT_DS_START, ACT_DS_SELECT,
    ACT_SWAP,      // swap the screens
    ACT_TOUCH,     // press the stylus at the on-screen cursor
    ACT_MIC,       // blow into the microphone
    ACT_COUNT
};

struct ActionDef
{
    const char* label;     // shown in CONTROLS
    const char* key;       // config.ini key
    const char* ingame;    // what it does in Mario Kart DS ("" = nothing useful)
    uint64_t    defaults;  // default Switch buttons (HidNpadButton bits)
};

// Switch buttons that can be bound (sticks/d-pad stay fixed).
struct SwitchButton
{
    const char* name;
    uint64_t    bit;
};

extern const SwitchButton kSwitchButtons[];
extern const int          kSwitchButtonCount;
extern const ActionDef    kActions[ACT_COUNT];

struct Settings
{
    // [video]
    int resolution  = Res::Auto;
    int layout      = Layout::Focus;
    int screen_size = Size::Fit;
    int filter      = Filter::Sharp;
    int effect      = Effect::None;
    int fps         = Corner::Off;
    // [gameplay]
    int fast_forward = FastFwd::X2;
    int language     = Lang::English;
    int cpu          = Cpu::Jit;
    int cpu_clock    = Clock::Boost;
    // [controls]
    uint64_t binds[ACT_COUNT] = {};
    // [cheats] names of enabled cheats
    std::vector<std::string> cheats_on;

    Settings();
    void reset_controls();
    bool cheat_enabled(const std::string& name) const;
    void set_cheat(const std::string& name, bool on);
};

extern const OptionDef kVideoOptions[];
extern const int       kVideoOptionCount;
extern const OptionDef kGameplayOptions[];
extern const int       kGameplayOptionCount;

bool config_load(Settings& s, const char* path);
bool config_save(const Settings& s, const char* path);

std::string buttons_to_string(uint64_t mask);   // "ZL,ZR" / "---"

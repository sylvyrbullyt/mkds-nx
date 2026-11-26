// Mario Kart DS NX - settings (config.ini)
#include "config.h"

#include <switch.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------
const SwitchButton kSwitchButtons[] = {
    {"A", HidNpadButton_A},   {"B", HidNpadButton_B},
    {"X", HidNpadButton_X},   {"Y", HidNpadButton_Y},
    {"L", HidNpadButton_L},   {"R", HidNpadButton_R},
    {"ZL", HidNpadButton_ZL}, {"ZR", HidNpadButton_ZR},
    {"PLUS", HidNpadButton_Plus}, {"MINUS", HidNpadButton_Minus},
};
const int kSwitchButtonCount = sizeof(kSwitchButtons) / sizeof(kSwitchButtons[0]);

const ActionDef kActions[ACT_COUNT] = {
    {"DS A",         "ds_a",      "ACCELERATE",      HidNpadButton_A},
    {"DS B",         "ds_b",      "BRAKE / REVERSE", HidNpadButton_B},
    {"DS X",         "ds_x",      "",                HidNpadButton_X},
    {"DS Y",         "ds_y",      "",                HidNpadButton_Y},
    {"DS L",         "ds_l",      "USE ITEM",        HidNpadButton_L},
    {"DS R",         "ds_r",      "HOP / DRIFT",     HidNpadButton_R},
    {"DS START",     "ds_start",  "PAUSE GAME",      HidNpadButton_Plus},
    {"DS SELECT",    "ds_select", "",                HidNpadButton_Minus},
    {"SWAP SCREENS", "swap",      "",                HidNpadButton_ZL},
    {"TOUCH",        "touch",     "",                HidNpadButton_ZR},
    {"MICROPHONE",   "mic",       "",                0},
};

static const Choice kResolution[] = {
    {"AUTO",  "auto",  "720p in handheld, 1080p when docked."},
    {"720P",  "720p",  "Always render at 1280x720."},
    {"1080P", "1080p", "Always render at 1920x1080."},
};
static const Choice kLayout[] = {
    {"FOCUS",        "focus",   "Big race screen, small map screen beside it."},
    {"SIDE BY SIDE", "side",    "Both screens the same size, next to each other."},
    {"STACKED",      "stacked", "Both screens on top of each other, like the DS."},
    {"ONE SCREEN",   "single",  "One screen at a time. Swap Screens flips it."},
};
static const Choice kScreenSize[] = {
    {"FIT",     "fit",     "As large as the screen allows."},
    {"INTEGER", "integer", "Largest whole-number scale: every pixel the same size."},
    {"90%",     "90",      "Slightly smaller, with a border."},
    {"80%",     "80",      "Smaller, with a border."},
    {"70%",     "70",      "Smallest, with a wide border."},
};
static const Choice kFilter[] = {
    {"SHARP",        "sharp",        "Crisp, blocky pixels."},
    {"SMOOTH",       "smooth",       "Soft, blended pixels."},
    {"SHARP+SMOOTH", "sharp_smooth", "Crisp pixels with softened uneven edges."},
};
static const Choice kEffect[] = {
    {"NONE",     "none",     "Clean picture."},
    {"LCD GRID", "lcd_grid", "Faint lines between pixels, like the DS screen."},
};
static const Choice kFps[] = {
    {"OFF",          "off",          "Hidden."},
    {"TOP LEFT",     "top_left",     "Shown in the top-left corner."},
    {"TOP RIGHT",    "top_right",    "Shown in the top-right corner."},
    {"BOTTOM LEFT",  "bottom_left",  "Shown in the bottom-left corner."},
    {"BOTTOM RIGHT", "bottom_right", "Shown in the bottom-right corner."},
};
static const Choice kFastForward[] = {
    {"OFF", "off", "Disabled."},
    {"2X",  "2x",  "Hold the right stick right: 2x speed."},
    {"3X",  "3x",  "Hold the right stick right: 3x speed."},
    {"4X",  "4x",  "Hold the right stick right: 4x speed."},
};
static const Choice kLanguage[] = {
    {"ENGLISH",  "english",  "Game text language. Applies the next time you start."},
    {"FRENCH",   "french",   "Game text language. Applies the next time you start."},
    {"SPANISH",  "spanish",  "Game text language. Applies the next time you start."},
    {"GERMAN",   "german",   "Not in the USA game: it will fall back to English."},
    {"ITALIAN",  "italian",  "Not in the USA game: it will fall back to English."},
    {"JAPANESE", "japanese", "Not in the USA game: it will fall back to English."},
};
static const Choice kCpu[] = {
    {"FAST (JIT)", "jit",         "Recompiles DS code: full speed. Applies next start."},
    {"SAFE",       "interpreter", "Slower but most compatible. Applies next start."},
};

static const Choice kClock[] = {
    {"1785 MHZ", "1785",  "Boosted CPU while playing: needed for full speed. More battery."},
    {"STOCK",    "stock", "System default 1020 MHz. Slower, but saves battery."},
};

#define OPT(label, key, table, field) {label, key, table, (int)(sizeof(table) / sizeof(table[0])), &Settings::field}

const OptionDef kVideoOptions[] = {
    OPT("RESOLUTION",    "resolution",  kResolution, resolution),
    OPT("SCREEN LAYOUT", "layout",      kLayout,     layout),
    OPT("SCREEN SIZE",   "screen_size", kScreenSize, screen_size),
    OPT("PIXEL FILTER",  "filter",      kFilter,     filter),
    OPT("SCREEN EFFECT", "effect",      kEffect,     effect),
    OPT("FPS COUNTER",   "fps",         kFps,        fps),
};
const int kVideoOptionCount = sizeof(kVideoOptions) / sizeof(kVideoOptions[0]);

const OptionDef kGameplayOptions[] = {
    OPT("FAST-FORWARD", "fast_forward", kFastForward, fast_forward),
    OPT("LANGUAGE",     "language",     kLanguage,    language),
    OPT("CPU MODE",     "cpu",          kCpu,         cpu),
    OPT("CPU CLOCK",    "cpu_clock",    kClock,       cpu_clock),
};
const int kGameplayOptionCount = sizeof(kGameplayOptions) / sizeof(kGameplayOptions[0]);

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
Settings::Settings() { reset_controls(); }

void Settings::reset_controls()
{
    for (int i = 0; i < ACT_COUNT; i++)
        binds[i] = kActions[i].defaults;
}

bool Settings::cheat_enabled(const std::string& name) const
{
    return std::find(cheats_on.begin(), cheats_on.end(), name) != cheats_on.end();
}

void Settings::set_cheat(const std::string& name, bool on)
{
    auto it = std::find(cheats_on.begin(), cheats_on.end(), name);
    if (on && it == cheats_on.end())
        cheats_on.push_back(name);
    else if (!on && it != cheats_on.end())
        cheats_on.erase(it);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static std::string upper(std::string s)
{
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

std::string buttons_to_string(uint64_t mask)
{
    std::string out;
    for (int i = 0; i < kSwitchButtonCount; i++)
    {
        if (!(mask & kSwitchButtons[i].bit)) continue;
        if (!out.empty()) out += ",";
        out += kSwitchButtons[i].name;
    }
    return out.empty() ? "---" : out;
}

static uint64_t buttons_from_string(const std::string& s)
{
    uint64_t mask = 0;
    size_t start = 0;
    while (start <= s.size())
    {
        size_t comma = s.find(',', start);
        if (comma == std::string::npos) comma = s.size();
        std::string name = upper(trim(s.substr(start, comma - start)));
        for (int i = 0; i < kSwitchButtonCount; i++)
            if (name == kSwitchButtons[i].name)
                mask |= kSwitchButtons[i].bit;
        start = comma + 1;
    }
    return mask;
}

static bool apply_option(Settings& s, const OptionDef* defs, int count,
                         const std::string& key, const std::string& value)
{
    for (int i = 0; i < count; i++)
    {
        if (key != defs[i].key) continue;
        for (int c = 0; c < defs[i].count; c++)
        {
            if (value == defs[i].choices[c].value)
            {
                s.*(defs[i].field) = c;
                return true;
            }
        }
        return false;   // known key, unknown value: keep the default
    }
    return false;
}

// ---------------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------------
bool config_load(Settings& s, const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return false;

    std::string section;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        std::string l = trim(line);
        if (l.empty() || l[0] == ';' || l[0] == '#') continue;
        if (l[0] == '[')
        {
            size_t end = l.find(']');
            section = l.substr(1, end == std::string::npos ? std::string::npos : end - 1);
            continue;
        }
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(l.substr(0, eq));
        std::string value = trim(l.substr(eq + 1));

        if (section == "video")
            apply_option(s, kVideoOptions, kVideoOptionCount, key, value);
        else if (section == "gameplay")
            apply_option(s, kGameplayOptions, kGameplayOptionCount, key, value);
        else if (section == "controls")
        {
            for (int i = 0; i < ACT_COUNT; i++)
                if (key == kActions[i].key)
                    s.binds[i] = buttons_from_string(value);
        }
        else if (section == "cheats")
        {
            if (value == "on") s.set_cheat(key, true);
        }
    }
    fclose(f);
    return true;
}

static void write_options(FILE* f, const Settings& s, const char* section,
                          const OptionDef* defs, int count)
{
    fprintf(f, "\n[%s]\n", section);
    for (int i = 0; i < count; i++)
    {
        std::string values;
        for (int c = 0; c < defs[i].count; c++)
        {
            if (c) values += " | ";
            values += defs[i].choices[c].value;
        }
        fprintf(f, "; %s: %s\n", defs[i].label, values.c_str());
        int cur = s.*(defs[i].field);
        if (cur < 0 || cur >= defs[i].count) cur = 0;
        fprintf(f, "%s=%s\n", defs[i].key, defs[i].choices[cur].value);
    }
}

bool config_save(const Settings& s, const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "; Mario Kart DS NX settings (edit here or in the Settings menu)\n");
    write_options(f, s, "video", kVideoOptions, kVideoOptionCount);
    write_options(f, s, "gameplay", kGameplayOptions, kGameplayOptionCount);

    fprintf(f, "\n[controls]\n; Switch buttons: A B X Y L R ZL ZR PLUS MINUS (comma-separated)\n");
    for (int i = 0; i < ACT_COUNT; i++)
        fprintf(f, "%s=%s\n", kActions[i].key,
                s.binds[i] ? buttons_to_string(s.binds[i]).c_str() : "");

    fprintf(f, "\n[cheats]\n; Names must match cheats.txt. on = enabled\n");
    for (const std::string& name : s.cheats_on)
        fprintf(f, "%s=on\n", name.c_str());

    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

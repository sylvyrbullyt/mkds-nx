// Mario Kart DS NX - cheats.txt
#include "cheats.h"

#include <cctype>
#include <cstdio>

static const char kTemplate[] =
    "; Mario Kart DS NX - cheat list\n"
    "; Add one cheat per line in the form:   Name = CODE\n"
    "; CODE is an Action Replay DS code: pairs of 8-digit hex numbers\n"
    "; (XXXXXXXX YYYYYYYY). Join the lines of a multi-line code with '+'.\n"
    "; Use codes made for the USA version of the game (AMCE).\n"
    "; Lines starting with ';' are ignored. Each cheat appears as an On/Off\n"
    "; toggle in Settings > Gameplay and takes effect the next time you launch.\n"
    "; Example layout (not a real code):\n"
    "; My Cheat = 12345678 00000000+D2000000 00000000\n";

static const size_t kMaxWords = 128;   // core limit: 64 code lines

static std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

// Normalises a code to "XXXXXXXX YYYYYYYY+..." and checks it is well formed.
static bool normalise(const std::string& in, std::string& out)
{
    std::vector<std::string> words;
    std::string cur;
    for (char c : in + " ")
    {
        if (isxdigit((unsigned char)c))
        {
            cur += (char)toupper((unsigned char)c);
        }
        else if (c == ' ' || c == '+' || c == '\t' || c == ',')
        {
            if (!cur.empty()) { words.push_back(cur); cur.clear(); }
        }
        else
        {
            return false;
        }
    }
    if (words.empty() || words.size() % 2 || words.size() > kMaxWords)
        return false;

    out.clear();
    for (size_t i = 0; i < words.size(); i++)
    {
        if (words[i].size() != 8) return false;
        if (i) out += (i % 2) ? " " : "+";
        out += words[i];
    }
    return true;
}

std::vector<Cheat> cheats_load(const char* path)
{
    std::vector<Cheat> list;

    FILE* f = fopen(path, "r");
    if (!f)
    {
        if ((f = fopen(path, "w")))
        {
            fputs(kTemplate, f);
            fclose(f);
        }
        return list;
    }

    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof(line), f))
    {
        lineno++;
        std::string l = trim(line);
        if (l.empty() || l[0] == ';' || l[0] == '#') continue;

        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;

        Cheat c;
        c.name = trim(l.substr(0, eq));
        if (c.name.empty() || !normalise(trim(l.substr(eq + 1)), c.code))
        {
            printf("cheats.txt line %d: bad cheat, skipped\n", lineno);
            continue;
        }
        list.push_back(c);
    }
    fclose(f);
    printf("Cheats: %zu loaded\n", list.size());
    return list;
}

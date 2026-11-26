// Mario Kart DS NX - cheats.txt
#pragma once

#include <string>
#include <vector>

struct Cheat
{
    std::string name;
    std::string code;   // "XXXXXXXX YYYYYYYY+XXXXXXXX YYYYYYYY" (core format)
};

// Reads cheats.txt, writing a commented template first if it does not exist.
std::vector<Cheat> cheats_load(const char* path);

// Mario Kart DS NX - ROM checks
#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RomStatus
{
    Ok,        // USA Mario Kart DS, header checksum good
    Warning,   // will still try to boot (other region, bad checksum, ...)
    Missing,
    Error,     // cannot boot (unreadable, archive, too small)
};

struct RomInfo
{
    RomStatus            status = RomStatus::Missing;
    std::string          message;   // one line for the title screen
    std::vector<uint8_t> data;      // whole ROM, handed to the core then freed
};

RomInfo rom_check(const char* path);

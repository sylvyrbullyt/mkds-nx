// Mario Kart DS NX - ROM checks
//
// Reads mkds.nds into memory and checks the cartridge header:
//   0x00C  game code   ("AMCE" = Mario Kart DS, USA)
//   0x15E  header CRC  (CRC-16/MODBUS of bytes 0x000-0x15D)
// Anything odd is reported as a warning and the game still boots, so a
// trimmed or patched dump is not blocked.
#include "rom.h"
#include "config.h"

#include <cstdio>
#include <cstring>

static const char  kGameCode[] = "AMCE";
static const size_t kMinSize   = 0x200;              // one header
static const size_t kMaxSize   = 512u * 1024 * 1024; // largest DS cart

static uint16_t crc16_modbus(const uint8_t* p, size_t n)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++)
    {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static const char* region_name(char c)
{
    switch (c)
    {
        case 'E': return "USA";
        case 'P': return "EUROPE";
        case 'J': return "JAPAN";
        case 'K': return "KOREA";
        case 'U': return "AUSTRALIA";
        default:  return "UNKNOWN REGION";
    }
}

RomInfo rom_check(const char* path)
{
    RomInfo info;

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("ROM not found: %s\n", path);
        info.status = RomStatus::Missing;
        info.message = "ROM NOT FOUND: PUT " ROM_FILE " IN /switch/mkds-nx/";
        return info;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < (long)kMinSize || (size_t)size > kMaxSize)
    {
        fclose(f);
        printf("ROM has an unexpected size (%ld bytes)\n", size);
        info.status = RomStatus::Error;
        info.message = "ROM ERROR: " ROM_FILE " IS NOT A DS ROM (BAD SIZE)";
        return info;
    }

    info.data.resize((size_t)size);
    size_t got = fread(info.data.data(), 1, info.data.size(), f);
    fclose(f);
    if (got != info.data.size())
    {
        printf("Could not read the ROM file (%zu of %ld bytes)\n", got, size);
        info.data.clear();
        info.status = RomStatus::Error;
        info.message = "ROM ERROR: COULD NOT READ " ROM_FILE;
        return info;
    }

    const uint8_t* h = info.data.data();
    if (!memcmp(h, "PK\x03\x04", 4) || !memcmp(h, "7z\xBC\xAF\x27\x1C", 6))
    {
        printf(ROM_FILE " is a zip/7z archive - unzip it on your computer first\n");
        info.data.clear();
        info.status = RomStatus::Error;
        info.message = "ROM ERROR: " ROM_FILE " IS A ZIP/7Z - UNZIP IT FIRST";
        return info;
    }

    char title[13] = {};
    char code[5] = {};
    memcpy(title, h + 0x000, 12);
    memcpy(code, h + 0x00C, 4);
    uint16_t want = (uint16_t)(h[0x15E] | (h[0x15F] << 8));
    uint16_t have = crc16_modbus(h, 0x15E);

    printf("ROM: title \"%s\", code %s, %ld KB, header CRC %04X (stored %04X)\n",
           title, code, size / 1024, have, want);

    if (memcmp(code, kGameCode, 3) != 0)
    {
        info.status = RomStatus::Warning;
        info.message = std::string("ROM WARNING: NOT MARIO KART DS (CODE ") + code + ")";
    }
    else if (code[3] != kGameCode[3])
    {
        info.status = RomStatus::Warning;
        info.message = std::string("ROM WARNING: ") + region_name(code[3]) +
                       " VERSION, EXPECTED USA";
    }
    else if (have != want)
    {
        info.status = RomStatus::Warning;
        info.message = "ROM WARNING: HEADER CHECKSUM MISMATCH (BAD DUMP?)";
    }
    else
    {
        info.status = RomStatus::Ok;
        info.message = "ROM OK  MARIO KART DS (USA)";
    }
    return info;
}

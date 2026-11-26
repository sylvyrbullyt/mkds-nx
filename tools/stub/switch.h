// Minimal stand-in for libnx, just enough for tools/preview.cpp on a desktop.
#pragma once
#include <cstdint>
typedef uint64_t u64; typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8;
enum { HidNpadButton_A=1<<0, HidNpadButton_B=1<<1, HidNpadButton_X=1<<2, HidNpadButton_Y=1<<3,
 HidNpadButton_L=1<<6, HidNpadButton_R=1<<7, HidNpadButton_ZL=1<<8, HidNpadButton_ZR=1<<9,
 HidNpadButton_Plus=1<<10, HidNpadButton_Minus=1<<11 };

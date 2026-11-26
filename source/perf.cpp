// Mario Kart DS NX - CPU clock while playing
//
// Homebrew runs at the stock 1020 MHz unless it asks for more. 1785 MHz is
// the clock the Switch itself uses while loading games ("boost mode"), so
// it is within what the hardware is designed to run; it does use more
// battery. Uses clkrst on firmware 8.0.0+ and pcv before that.
#include "perf.h"

#include <switch.h>

#include <cstdio>

static u32  g_saved_hz = 0;   // rate before we changed it (0 = untouched)
static bool g_changed = false;

static bool get_rate(u32* hz)
{
    Result rc;
    if (hosversionAtLeast(8, 0, 0))
    {
        if (R_FAILED(rc = clkrstInitialize())) return false;
        ClkrstSession s;
        if (R_SUCCEEDED(rc = clkrstOpenSession(&s, PcvModuleId_CpuBus, 3)))
        {
            rc = clkrstGetClockRate(&s, hz);
            clkrstCloseSession(&s);
        }
        clkrstExit();
    }
    else
    {
        if (R_FAILED(rc = pcvInitialize())) return false;
        rc = pcvGetClockRate(PcvModule_CpuBus, hz);
        pcvExit();
    }
    return R_SUCCEEDED(rc);
}

static bool set_rate(u32 hz)
{
    Result rc;
    if (hosversionAtLeast(8, 0, 0))
    {
        if (R_FAILED(rc = clkrstInitialize())) return false;
        ClkrstSession s;
        if (R_SUCCEEDED(rc = clkrstOpenSession(&s, PcvModuleId_CpuBus, 3)))
        {
            rc = clkrstSetClockRate(&s, hz);
            clkrstCloseSession(&s);
        }
        clkrstExit();
    }
    else
    {
        if (R_FAILED(rc = pcvInitialize())) return false;
        rc = pcvSetClockRate(PcvModule_CpuBus, hz);
        pcvExit();
    }
    if (R_FAILED(rc)) printf("CPU clock: set %u Hz failed (0x%x)\n", hz, rc);
    return R_SUCCEEDED(rc);
}

void perf_set_cpu(int mhz)
{
    if (mhz <= 0)
    {
        perf_restore();
        return;
    }
    if (!g_changed && !get_rate(&g_saved_hz)) g_saved_hz = 0;
    if (set_rate((u32)mhz * 1000000u))
    {
        g_changed = true;
        u32 now = 0;
        get_rate(&now);
        printf("CPU clock: %u MHz (was %u MHz)\n", now / 1000000, g_saved_hz / 1000000);
    }
}

void perf_restore()
{
    if (!g_changed) return;
    set_rate(g_saved_hz ? g_saved_hz : 1020000000u);
    g_changed = false;
    printf("CPU clock: restored\n");
}

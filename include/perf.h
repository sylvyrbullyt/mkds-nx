// Mario Kart DS NX - CPU clock while playing
#pragma once

// Raises the CPU to `mhz` (0 = leave the system clock alone) and remembers
// the previous rate; perf_restore() puts it back. Safe to call repeatedly.
void perf_set_cpu(int mhz);
void perf_restore();

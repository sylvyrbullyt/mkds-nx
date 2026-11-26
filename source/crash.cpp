// Mario Kart DS NX - last-resort crash handler
//
// melonDS installs the libnx exception handler so its JIT can recover from
// fast-memory faults. Faults it cannot handle land here. We log where it
// happened and exit; running.flag is left behind, so the next launch shows
// "LAST GAME DID NOT CLOSE CLEANLY" and suggests CPU MODE: SAFE.
#include <switch.h>

#include <cstdio>

void HandleFault(u64 pc, u64 lr, u64 fp, u64 faultAddr, u32 desc)
{
    extern char __start__;
    u64 base = (u64)&__start__;
    printf("CRASH: pc=%016lx (app+%lx) lr=%016lx (app+%lx) fp=%016lx addr=%016lx desc=%x\n",
           pc, pc - base, lr, lr - base, fp, faultAddr, desc);
    fflush(stdout);
    svcExitProcess();
}

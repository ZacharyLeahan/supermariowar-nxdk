#include "RuntimeDiagnostics.h"
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/xbox.h>
#include <cstdio>

#ifdef NXDK_RUNTIME_WATCHDOG_MS
static_assert(NXDK_RUNTIME_WATCHDOG_MS >= 10000 && NXDK_RUNTIME_WATCHDOG_MS <= 120000);
namespace {
volatile LONG armed=0, serial=0, frame=0, phase=0, state=0, context=-1;
LONG read(volatile LONG *value) { return InterlockedCompareExchange(value, 0, 0); }
DWORD WINAPI watch(void *)
{
    LONG last=read(&serial);
    DWORD changed=GetTickCount();
    while (read(&armed)) {
        Sleep(100);
        if (!read(&armed)) break;
        const LONG now=read(&serial);
        if (now!=last) { last=now; changed=GetTickCount(); continue; }
        const DWORD elapsed=GetTickCount()-changed;
        if (elapsed < NXDK_RUNTIME_WATCHDOG_MS) continue;
        // Do not call SDL, stdout or the save system after a suspected main-loop
        // stall. Scalar snapshots can straddle a checkpoint; the stage is a
        // localization hint, not a crash stack or an atomic world snapshot.
        char marker[256];
        const int n=snprintf(marker,sizeof(marker),
            "runtime watchdog timeout elapsed_ms=%lu serial=%ld frame=%ld phase=%ld appstate=%ld context=%ld\n",
            (unsigned long)elapsed,(long)now,(long)read(&frame),(long)read(&phase),
            (long)read(&state),(long)read(&context));
        // Preserve any earlier failure marker rather than overwrite it.
        HANDLE f=CreateFileA("D:\\runtime-watchdog.log",GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
        if (f!=INVALID_HANDLE_VALUE) {
            DWORD written=0;
            if(n>0 && n<(int)sizeof(marker)) WriteFile(f,marker,DWORD(n),&written,nullptr);
            IO_STATUS_BLOCK io{}; NtFlushBuffersFile(f,&io); CloseHandle(f);
        }
        XReboot();
        return 0;
    }
    return 0;
}
}
void nxdk_runtime_watchdog_start()
{
    if (InterlockedExchange(&armed,1)) return;
    HANDLE thread=CreateThread(nullptr,65536,watch,nullptr,0,nullptr);
    if (thread) {
        CloseHandle(thread);
        printf("[runtime] diagnostic watchdog armed timeout_ms=%u\n",unsigned(NXDK_RUNTIME_WATCHDOG_MS));
    } else {
        InterlockedExchange(&armed,0);
        fprintf(stderr,"[runtime] diagnostic watchdog could not start\n");
    }
}
void nxdk_runtime_watchdog_stop() { InterlockedExchange(&armed,0); }
void nxdk_runtime_checkpoint(NxdkRuntimeStage stage,long appstate,long detail)
{
    InterlockedExchange(&phase,LONG(stage));
    InterlockedExchange(&state,LONG(appstate));
    InterlockedExchange(&context,LONG(detail));
    if(stage==NxdkRuntimeStage::frame_start) InterlockedIncrement(&frame);
    InterlockedIncrement(&serial);
}
#endif

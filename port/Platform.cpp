#include "Platform.h"
#include "util/Error.h"
#include "GameValues.h"
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <cstdio>
#include <string>
#include <SDL.h>
extern std::string RootDataDirectory;
extern CGameValues game_values;
extern short joystickcount;
extern SDL_Joystick** joysticks;
extern void nxdk_audio_update();
#ifdef NXDK_AUDIO_CLOSE_EXPERIMENT
extern "C" void nxdk_audio_close_load_configuration(void);
#endif
#ifdef NXDK_AUDIO_START_SETTLE_EXPERIMENT
extern "C" void nxdk_audio_settle_load_configuration(void);
#endif
void nxdk_poll_recovery() {
    nxdk_audio_update();
    // The KVM can reset/re-enumerate its virtual pad independently of the game.
    // Reopen handles when the connected instance list changes, including when
    // a replacement pad occupies the same device index.
    const short count=(short)SDL_NumJoysticks();
    bool changed=count!=joystickcount;
    for(short i=0;!changed && i<count;i++)
        changed=!joysticks[i] || !SDL_JoystickGetAttached(joysticks[i]) ||
            SDL_JoystickInstanceID(joysticks[i])!=SDL_JoystickGetDeviceInstanceID(i);
    if(changed) {
        for(short i=0;i<joystickcount;i++) if(joysticks[i]) SDL_JoystickClose(joysticks[i]);
        delete[] joysticks;
        joystickcount=count;
        joysticks=new SDL_Joystick*[count]{};
        for(short i=0;i<count;i++) joysticks[i]=SDL_JoystickOpen(i);
        game_values.playerInput.ResetKeys();
        nxdk_configure_controllers();
    }
    static Uint32 held=0;
    bool reset=false;
    for (short i=0;i<joystickcount;i++) {
        if (joysticks[i] && SDL_JoystickGetButton(joysticks[i],6) && SDL_JoystickGetButton(joysticks[i],7)) reset=true;
    }
    if (!reset) held=0;
    else if (!held) held=SDL_GetTicks();
    else if (SDL_GetTicks()-held>=2000) { fflush(nullptr); XReboot(); }
}
bool nxdk_controller_connected(int slot) {
    return slot >= 0 && slot < 4 && nxdk_joystick_for_slot(slot) != nullptr;
}
bool nxdk_controller_button_down(int slot, int button) {
    SDL_Joystick* joystick = slot >= 0 && slot < 4 ? nxdk_joystick_for_slot(slot) : nullptr;
    return joystick && SDL_JoystickGetButton(joystick, button);
}
namespace {
[[noreturn]] void waitForXboxTransition() {
    for (;;) Sleep(1000);
}
void prepareXboxTransition(const char* action) {
    printf("[exit] %s\n", action);
    fflush(nullptr);
    Sleep(100);
}
}
[[noreturn]] void nxdk_return_to_dashboard() {
    prepareXboxTransition("return to dashboard");
    XLaunchXBEEx(nullptr, nullptr);
    waitForXboxTransition();
}
[[noreturn]] void nxdk_restart_xbox() {
    prepareXboxTransition("restart Xbox");
    XReboot();
    waitForXboxTransition();
}
[[noreturn]] void nxdk_power_off_xbox() {
    prepareXboxTransition("power off Xbox");
    HalInitiateShutdown();
    waitForXboxTransition();
}
void nxdk_log_memory(const char* stage) {
    MM_STATISTICS stats = {}; stats.Length=sizeof(stats);
    if (MmQueryStatistics(&stats) >= 0)
        printf("[memory] %s: total=%lu KiB available=%lu KiB image=%lu KiB\n",stage,
            stats.TotalPhysicalPages*4,stats.AvailablePages*4,stats.ImagePagesCommitted*4);
}
#ifdef NXDK_STARTUP_WATCHDOG_MS
static_assert(NXDK_STARTUP_WATCHDOG_MS >= 1000 && NXDK_STARTUP_WATCHDOG_MS <= 120000);
namespace {
volatile LONG startupStage = LONG(NxdkStartupStage::setup);
DWORD WINAPI startupWatchdog(void *)
{
    const DWORD began = GetTickCount();
    while (GetTickCount() - began < NXDK_STARTUP_WATCHDOG_MS) {
        if (InterlockedCompareExchange(&startupStage, 0, 0) == LONG(NxdkStartupStage::ready)) return 0;
        Sleep(100);
    }
    const LONG stage = InterlockedCompareExchange(&startupStage, 0, 0);
    if (stage == LONG(NxdkStartupStage::ready)) return 0;
    HANDLE f = CreateFileA("D:\\startup-watchdog.log", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        char marker[] = "startup watchdog timeout stage=0\n";
        marker[sizeof(marker)-3] = char('0' + stage);
        DWORD n = 0;
        WriteFile(f, marker, sizeof(marker)-1, &n, nullptr);
        IO_STATUS_BLOCK io{};
        NtFlushBuffersFile(f, &io);
        CloseHandle(f);
    }
    XReboot();
    return 0;
}
}
void nxdk_startup_checkpoint(NxdkStartupStage stage)
{
    InterlockedExchange(&startupStage, LONG(stage));
    printf("[startup] checkpoint=%ld\n", long(stage));
#ifdef NXDK_STARTUP_TEST_HANG_STAGE
    static_assert(NXDK_STARTUP_TEST_HANG_STAGE >= 1 && NXDK_STARTUP_TEST_HANG_STAGE <= 7);
    if (long(stage) == NXDK_STARTUP_TEST_HANG_STAGE) {
        printf("[startup] diagnostic injected wait stage=%ld\n", long(stage));
        for (;;) Sleep(100);
    }
#endif
}
#endif
bool nxdk_create_directory(const char* path) {
    const DWORD flags=GetFileAttributesA(path);
    if(flags!=INVALID_FILE_ATTRIBUTES) return (flags & FILE_ATTRIBUTE_DIRECTORY)!=0;
    if(CreateDirectoryA(path,nullptr)) return true;
    const DWORD after=GetFileAttributesA(path);
    return after!=INVALID_FILE_ATTRIBUTES && (after & FILE_ATTRIBUTE_DIRECTORY)!=0;
}
void nxdk_startup() {
#ifdef NXDK_STARTUP_WATCHDOG_MS
    HANDLE startupGuard = CreateThread(nullptr, 65536, startupWatchdog, nullptr, 0, nullptr);
    const bool startupGuardStarted = startupGuard != nullptr;
    if (startupGuard) CloseHandle(startupGuard);
#endif
    XVideoSetMode(640,480,16,REFRESH_DEFAULT);
    // Title-local diagnostics; failure to open a log does not prevent boot.
    freopen("D:\\smw.log", "w", stdout);
    freopen("D:\\smw-error.log", "w", stderr);
    setvbuf(stdout,nullptr,_IONBF,0);
    setvbuf(stderr,nullptr,_IONBF,0);
#ifdef NXDK_AUDIO_START_SETTLE_EXPERIMENT
    nxdk_audio_settle_load_configuration();
#endif
#ifdef NXDK_AUDIO_CLOSE_EXPERIMENT
    nxdk_audio_close_load_configuration();
#endif
    RootDataDirectory="D:/data/";
    debugPrint("Super Mario War nxdk: loading...\n");
    nxdk_log_memory("startup");
#ifdef NXDK_STARTUP_WATCHDOG_MS
    if (startupGuardStarted)
        printf("[startup] diagnostic watchdog armed timeout_ms=%u\n", unsigned(NXDK_STARTUP_WATCHDOG_MS));
    else fprintf(stderr, "[startup] diagnostic watchdog could not start\n");
#endif
}
namespace smw {
[[noreturn]] void fatal(const char* message) {
    fprintf(stderr,"FATAL: %s\n",message);
    fflush(nullptr);
    debugPrint("SMW fatal error:\n%s\nSee D:\\smw-error.log\n",message);
    // Preserve diagnostics, then recover even when a BIOS IGR hook is unavailable.
    debugPrint("Restarting Xbox in 60 seconds (or hold Back + Start).\n");
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_Joystick* pads[4] = {};
    for (int i=0;i<SDL_NumJoysticks() && i<4;i++) pads[i]=SDL_JoystickOpen(i);
    const Uint32 started=SDL_GetTicks();
    Uint32 held=0;
    while (SDL_GetTicks()-started < 60000) {
        SDL_PumpEvents();
        SDL_JoystickUpdate();
        bool reset=false;
        for (auto* pad:pads)
            if (pad && SDL_JoystickGetButton(pad,6) && SDL_JoystickGetButton(pad,7)) reset=true;
        if (!reset) held=0;
        else if (!held) held=SDL_GetTicks();
        else if (SDL_GetTicks()-held>=2000) break;
        Sleep(20);
    }
    XReboot();
    for (;;) Sleep(1000);
}
}
void nxdk_configure_controllers(bool installDefaults) {
    printf("[input] %d controllers detected\n",joystickcount);
    for(short i=0;i<joystickcount;i++)
        printf("[input] index=%d instance=%ld physical_port=%d slot=%d name=%s open=%d\n",i,
            joysticks[i] ? (long)SDL_JoystickInstanceID(joysticks[i]) : -1L,
            joysticks[i] ? SDL_JoystickGetPlayerIndex(joysticks[i]) : -1,
            joysticks[i] ? nxdk_joystick_slot(SDL_JoystickInstanceID(joysticks[i])) : -1,
            SDL_JoystickNameForIndex(i),joysticks[i]!=nullptr);
    printf("[input] controller setup=%s\n", installDefaults ? "initial defaults" : "preserve choices and bindings");
    // Re-enumeration must not replace the user's player choices, device
    // assignments or custom bindings. An absent pad retains its player slot;
    // reconnect restores input without silently converting the match to bots.
    if (!installDefaults) return;
    short connectedSlots[4]{};
    short connected = 0;
    for (short slot = 0; slot < 4; ++slot)
        if (nxdk_joystick_for_slot(slot)) connectedSlots[connected++] = slot;
    for (short i=0;i<4;i++) {
        auto& control=game_values.inputConfiguration[i][1];
        control.iDevice = i < connected ? connectedSlots[i] : i;
        game_values.playerInput.inputControls[i]=&control;
        // Keep player one available even if USB enumeration finishes after
        // startup. Reconnect preserves this setup rather than promoting bots.
        game_values.playercontrol[i]= (i==0 || i<connected) ? 1 : (i==1 && connected<2 ? 2 : 0);
        auto& game=control.inputGameControls[0];
        auto& menu=control.inputGameControls[1];
        // nxdk SDL2: A=0 B=1 X=2 Y=3 Back=6 Start=7, D-pad=hat 0.
        const SDL_Keycode gameplay[8]={JOY_HAT_LEFT,JOY_HAT_RIGHT,JOY_BUTTON_START,JOY_HAT_DOWN,JOY_BUTTON_START+2,JOY_BUTTON_START+3,JOY_BUTTON_START+7,JOY_BUTTON_START+1};
        const SDL_Keycode navigation[8]={JOY_HAT_UP,JOY_HAT_DOWN,JOY_HAT_LEFT,JOY_HAT_RIGHT,JOY_BUTTON_START,JOY_BUTTON_START+1,JOY_BUTTON_START+2,JOY_BUTTON_START+3};
        for(int k=0;k<8;k++){game.keys[k]=gameplay[k];menu.keys[k]=navigation[k];}
    }
}

#ifdef NXDK_EXIT_WATCHDOG_MS
static_assert(NXDK_EXIT_WATCHDOG_MS >= 1000 && NXDK_EXIT_WATCHDOG_MS <= 60000);
namespace {
DWORD WINAPI exitWatchdog(void *) {
    Sleep(NXDK_EXIT_WATCHDOG_MS);
    // Do not use stdio/SDL during teardown. This marker invalidates clean-exit
    // acceptance if the watchdog recovers a hung shutdown.
    HANDLE file=CreateFileA("D:\\exit-watchdog.log",GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file!=INVALID_HANDLE_VALUE) {
        const char marker[]="normal exit watchdog fired\n";
        DWORD written=0;
        WriteFile(file,marker,sizeof(marker)-1,&written,nullptr);
        IO_STATUS_BLOCK io{};
        NtFlushBuffersFile(file,&io);
        CloseHandle(file);
    }
    XReboot();
    return 0;
}
}
#endif
void nxdk_guard_normal_exit() {
#ifdef NXDK_EXIT_WATCHDOG_MS
    static bool armed=false;
    if(armed) return;
    HANDLE thread=CreateThread(nullptr,65536,exitWatchdog,nullptr,0,nullptr);
    if(thread) {
        CloseHandle(thread);
        armed=true;
        printf("[exit] diagnostic watchdog armed timeout_ms=%u\n",unsigned(NXDK_EXIT_WATCHDOG_MS));
    } else {
        fprintf(stderr,"[exit] diagnostic watchdog could not start\n");
    }
#endif
}

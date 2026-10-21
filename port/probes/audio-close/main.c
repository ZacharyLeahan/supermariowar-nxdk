/* Silent fault fixture: withhold HAL notifications, then close SDL output. */
#include <SDL.h>
#include <hal/audio.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <string.h>
extern AC97_DEVICE ac97Device;
static void raw_state(const char *phase)
{
    const volatile unsigned char *r = (const volatile unsigned char *)0xfec00000;
    const volatile unsigned short *r16 = (const volatile unsigned short *)r;
    const volatile unsigned long *r32 = (const volatile unsigned long *)r;
    printf("phase=%s global_control=%08lx global_status=%08lx "
           "analog_civ=%u lvi=%u remaining=%u digital_civ=%u lvi=%u remaining=%u\n",
           phase, r32[0x12c / 4], r32[0x130 / 4],
           r[0x114], r[0x115], r16[0x118 / 2], r[0x174], r[0x175], r16[0x178 / 2]);
}
static volatile LONG app_callbacks, withheld, close_finished;
static void SDLCALL fill(void *unused, Uint8 *stream, int len)
{
    (void)unused;
    memset(stream, 0, len);
    InterlockedIncrement(&app_callbacks);
}
static void suppress_completion(void *device, void *data)
{
    (void)device; (void)data;
    /* DPC context: integer atomics only; intentionally no semaphore post. */
    InterlockedIncrement(&withheld);
}
static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;
    Sleep(3000);
    if (InterlockedCompareExchange(&close_finished, 0, 0)) return 0;
    HANDLE f = CreateFileA("D:\\close-watchdog.log", GENERIC_WRITE, 0, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        const char marker[] = "audio close exceeded 3000 ms; watchdog reboot\n";
        DWORD written;
        WriteFile(f, marker, sizeof(marker) - 1, &written, NULL);
        IO_STATUS_BLOCK io;
        NtFlushBuffersFile(f, &io);
        CloseHandle(f);
    }
    XReboot();
    return 0;
}
int main(void)
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);
    freopen("D:\\audio-close.log", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Silent SDL close fault probe; suppress real completion notifications\n");
    raw_state("before-sdl");
    if (SDL_Init(SDL_INIT_AUDIO) < 0) goto failed;
    SDL_AudioSpec want = {0}, have = {0};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2;
    want.samples = 1024; want.callback = fill;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!device) goto failed;
    SDL_PauseAudioDevice(device, 0);
    SDL_Delay(200);
    LONG initial = InterlockedCompareExchange(&app_callbacks, 0, 0);
    printf("startup_callbacks=%ld\n", initial);
    raw_state("before-fault");
    SDL_Delay(100);
    LONG confirmed = InterlockedCompareExchange(&app_callbacks, 0, 0);
    printf("pre_fault_progress before=%ld after=%ld\n", initial, confirmed);
    if (!initial || confirmed <= initial) {
        raw_state("stalled-before-injection");
        printf("result=inconclusive startup stalled before fault injection\n");
        XReboot(); return 1;
    }
    /* Atomic pointer-sized replacement on this single-CPU target. This changes
       notifications only; it does not simulate consumption or write DMA state. */
    ac97Device.callback = suppress_completion;
    SDL_Delay(200);
    LONG before = InterlockedCompareExchange(&app_callbacks, 0, 0);
    SDL_Delay(100);
    LONG after = InterlockedCompareExchange(&app_callbacks, 0, 0);
    LONG blocked = InterlockedCompareExchange(&withheld, 0, 0);
    raw_state("after-fault");
    printf("fault_callbacks_before=%ld after=%ld withheld=%ld\n", before, after, blocked);
    if (!blocked || before != after) { printf("result=inconclusive fault did not block output\n"); XReboot(); return 1; }
    HANDLE guard = CreateThread(NULL, 65536, watchdog, NULL, 0, NULL);
    if (!guard) { printf("result=inconclusive watchdog unavailable\n"); XReboot(); return 1; }
    CloseHandle(guard);
    printf("stage=close-enter watchdog_ms=3000\n");
    Uint32 start = SDL_GetTicks();
    SDL_CloseAudioDevice(device);
    InterlockedExchange(&close_finished, 1);
    printf("stage=close-return elapsed_ms=%lu\n", (unsigned long)(SDL_GetTicks() - start));
    printf("result=closed-after-withheld-notifications\n");
    Sleep(500);
    XReboot();
    return 0;
failed:
    printf("result=inconclusive SDL_error=%s\n", SDL_GetError());
    Sleep(500);
    XReboot();
    return 1;
}

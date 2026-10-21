/* Minimal silent SDL2 callback/AC97 progress probe; no game or decoder code. */
#include <SDL.h>
#include "../../PciAudioState.h"
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void raw_state(const char *phase)
{
    const volatile uint8_t *r = (const volatile uint8_t *)0xfec00000;
    const volatile uint16_t *r16 = (const volatile uint16_t *)r;
    const volatile uint32_t *r32 = (const volatile uint32_t *)r;
    printf("phase=%s global_control=%08lx global_status=%08lx "
           "analog_base=%08lx remaining=%u prefetched=%u "
           "digital_base=%08lx remaining=%u prefetched=%u\n", phase,
           (unsigned long)r32[0x12c / 4], (unsigned long)r32[0x130 / 4],
           (unsigned long)r32[0x110 / 4], r16[0x118 / 2], r[0x11a],
           (unsigned long)r32[0x170 / 4], r16[0x178 / 2], r[0x17a]);
}
static unsigned long callbacks;
static void SDLCALL fill(void *unused, Uint8 *stream, int len)
{
    (void)unused;
    memset(stream, 0, len);
    ++callbacks;
}
static void report(SDL_AudioDeviceID device, unsigned elapsed)
{
    SDL_LockAudioDevice(device);
    unsigned long count = callbacks;
    SDL_UnlockAudioDevice(device);
    const volatile uint8_t *regs = (const volatile uint8_t *)0xfec00000;
    printf("elapsed_ms=%u callbacks=%lu analog_civ=%u lvi=%u status=%u control=%u "
           "digital_civ=%u lvi=%u status=%u control=%u\n", elapsed, count,
           regs[0x114], regs[0x115], regs[0x116], regs[0x11b],
           regs[0x174], regs[0x175], regs[0x176], regs[0x17b]);
    raw_state("running");
}
int main(void)
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);
    freopen("D:\\audio-startup.log", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Silent nxdk SDL2 startup probe; no audibility test\n");
    raw_state("before-sdl");
    nxdk_log_pci_audio("before-sdl");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
        goto failed;
    SDL_Window *window = SDL_CreateWindow("audio startup", 0, 0, 640, 480, 0);
    if (!window)
        goto failed;
    SDL_AudioSpec want = {0}, have = {0};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = fill;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!device)
        goto failed;
    printf("driver=%s frequency=%d channels=%u samples=%u\n",
           SDL_GetCurrentAudioDriver(), have.freq, have.channels, have.samples);
    raw_state("opened");
    nxdk_log_pci_audio("opened");
    SDL_PauseAudioDevice(device, 0);
    for (unsigned elapsed = 0; elapsed <= 10000; elapsed += 1000) {
        report(device, elapsed);
        SDL_Delay(1000);
    }
    SDL_LockAudioDevice(device);
    const unsigned long final_count = callbacks;
    SDL_UnlockAudioDevice(device);
    nxdk_log_pci_audio("after-playback");
    printf("result=%s (callback progress only)\n", final_count ? "progress" : "stalled");
    /* A blocked driver wait may hang SDL_CloseAudioDevice; reboot is bounded. */
    fflush(stdout);
    XReboot();
    return 0;
failed:
    printf("SDL failure: %s\n", SDL_GetError());
    SDL_Delay(3000);
    XReboot();
    return 1;
}

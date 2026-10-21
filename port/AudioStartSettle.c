// SPDX-License-Identifier: MIT
/* Paired startup-timing experiment. The same image selects 0 or 1000 us via
 * a title-local file read BEFORE audio initialization, never in the callback.
 * Include the selected SDK's unmodified HAL; no SDK source/library mutation. */
#include <hal/audio.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned settle_us;
static int pending_start;
void nxdk_audio_settle_load_configuration(void)
{
    char data[8];
    FILE *file = fopen("D:\\audio-settle.txt", "rb");
    const char *state = "missing";
    settle_us = 0;
    if (file) {
        const size_t n = fread(data, 1, sizeof(data), file);
        const int error = ferror(file);
        fclose(file);
        if (!error && ((n == 1 && !memcmp(data, "0", 1)) || (n == 2 && !memcmp(data, "0\n", 2)))) {
            state = "valid";
        } else if (!error && ((n == 4 && !memcmp(data, "1000", 4)) || (n == 5 && !memcmp(data, "1000\n", 5)))) {
            settle_us = 1000;
            state = "valid";
        } else {
            state = "invalid";
            fprintf(stderr, "[audio-settle] invalid configuration; using zero-delay control\n");
        }
    }
    printf("[audio-settle] configuration=%s requested_us=%u experimental=1\n", state, settle_us);
}

#define XAudioInit original_XAudioInit
#define XAudioPlay original_XAudioPlay
#include <hal/audio.c>
#undef XAudioPlay
#undef XAudioInit

void XAudioInit(int bits, int channels, XAudioCallback callback, void *data)
{
    pending_start = 1;
    original_XAudioInit(bits, channels, callback, data);
}
void XAudioPlay(void)
{
    if (!pending_start) {
        original_XAudioPlay();
        return;
    }
    pending_start = 0;
    const uint64_t before = KeQueryPerformanceCounter();
    if (settle_us) KeStallExecutionProcessor(settle_us);
    const uint64_t ticks = KeQueryPerformanceCounter() - before;
    original_XAudioPlay();
    // Log AFTER starting DMA: neither control arm adds a printf before start.
    printf("[audio-settle] start requested_us=%u elapsed_ticks=%llu frequency_hz=%llu\n",
           settle_us, (unsigned long long)ticks,
           (unsigned long long)KeQueryPerformanceFrequency());
}

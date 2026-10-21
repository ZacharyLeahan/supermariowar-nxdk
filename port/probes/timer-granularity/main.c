/* Measure kernel wait primitives; no SDL/audio initialization or AC97 writes. */
#include <hal/video.h>
#include <hal/xbox.h>
#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define SAMPLE_COUNT 1000
static uint64_t samples[SAMPLE_COUNT];
static volatile LONG finished;
static int compare(const void *a, const void *b)
{
    const uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x > y ? 1 : x < y ? -1 : 0;
}
static DWORD WINAPI guard(void *unused)
{
    (void)unused;
    Sleep(10000);
    if (InterlockedCompareExchange(&finished, 0, 0)) return 0;
    HANDLE f = CreateFileA("D:\\timer-watchdog.log", GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        const char marker[] = "timer measurement exceeded 10 seconds\n";
        DWORD n;
        WriteFile(f, marker, sizeof(marker)-1, &n, NULL);
        IO_STATUS_BLOCK io;
        NtFlushBuffersFile(f, &io);
        CloseHandle(f);
    }
    XReboot();
    return 0;
}
static void measure(unsigned kind, unsigned us, uint64_t frequency)
{
    unsigned under = 0, errors = 0;
    uint64_t total = 0;
    LARGE_INTEGER interval;
    interval.QuadPart = -(LONGLONG)us * 10;
    for (unsigned i = 0; i < SAMPLE_COUNT; ++i) {
        const uint64_t begin = KeQueryPerformanceCounter();
        NTSTATUS status = 0;
        if (kind == 1) status = KeDelayExecutionThread(KernelMode, FALSE, &interval);
        else if (kind == 2) KeStallExecutionProcessor(us);
        samples[i] = KeQueryPerformanceCounter() - begin;
        total += samples[i];
        errors += status != 0;
        // Conservative: include one full counter tick of uncertainty. The
        // measured interval also includes call/branch overhead, never subtracts it.
        under += us && (samples[i] + 1) * 1000000ULL < frequency * us;
    }
    qsort(samples, SAMPLE_COUNT, sizeof(samples[0]), compare);
    printf("kind=%s requested_us=%u samples=%u min_ticks=%llu median_ticks=%llu "
           "p95_ticks=%llu max_ticks=%llu total_ticks=%llu definite_under_target=%u errors=%u\n",
        kind == 0 ? "counter-baseline" : kind == 1 ? "thread-delay" : "processor-stall",
        us, SAMPLE_COUNT, (unsigned long long)samples[0],
        (unsigned long long)samples[SAMPLE_COUNT/2],
        (unsigned long long)samples[SAMPLE_COUNT*95/100],
        (unsigned long long)samples[SAMPLE_COUNT-1], (unsigned long long)total, under, errors);
}
int main(void)
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);
    freopen("D:\\timer-granularity.log", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Kernel timer measurement; no SDL/audio initialization or AC97 writes\n");
    HANDLE thread = CreateThread(NULL, 65536, guard, NULL, 0, NULL);
    if (!thread) { printf("result=inconclusive watchdog unavailable\n"); XReboot(); return 1; }
    CloseHandle(thread);
    const uint64_t frequency = KeQueryPerformanceFrequency();
    printf("counter_frequency_hz=%llu\n", (unsigned long long)frequency);
    if (!frequency) { printf("result=inconclusive counter unavailable\n"); XReboot(); return 1; }
    measure(0, 0, frequency);
    measure(1, 1, frequency);
    measure(1, 10, frequency);
    measure(1, 100, frequency);
    measure(2, 1, frequency);
    measure(2, 10, frequency);
    printf("result=measurement-complete not-an-audio-or-reset-pulse-test\n");
    fclose(stdout);
    InterlockedExchange(&finished, 1);
    XReboot();
    return 0;
}

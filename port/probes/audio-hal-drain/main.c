/* Silent direct-HAL experiment: do completions resume after a drained queue? */
#include <hal/audio.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static volatile LONG completions;
static void completed(void *device, void *unused)
{
    (void)device; (void)unused;
    InterlockedIncrement(&completions);
}
static void report(unsigned submitted)
{
    const volatile uint8_t *r = (const volatile uint8_t *)0xfec00000;
    printf("submitted=%u callbacks=%ld analog_civ=%u lvi=%u status=%u control=%u "
           "digital_civ=%u lvi=%u status=%u control=%u\n", submitted,
           InterlockedCompareExchange(&completions, 0, 0),
           r[0x114],r[0x115],r[0x116],r[0x11b],r[0x174],r[0x175],r[0x176],r[0x17b]);
}
int main(void)
{
    XVideoSetMode(640,480,16,REFRESH_DEFAULT);
    freopen("D:\\hal-drain.log","w",stdout);
    setvbuf(stdout,NULL,_IONBF,0);
    printf("Silent HAL drain/resume probe: 4096 bytes per buffer; 100 ms gaps\n");
    void *pcm = MmAllocateContiguousMemoryEx(4096,0,0xffffffff,0,PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!pcm) { printf("allocation failed\n"); Sleep(1000); XReboot(); return 1; }
    memset(pcm,0,4096);
    XAudioInit(16,2,completed,NULL);
    for(unsigned i=1;i<=8;++i) {
        // A single 21 ms silent buffer drains before the next submission.
        XAudioProvideSamples(pcm,4096,0);
        XAudioPlay();
        Sleep(100);
        report(i);
    }
    printf("done; callback count should continue to advance after each resubmission\n");
    fflush(stdout);
    Sleep(500);
    XReboot();
    return 0;
}

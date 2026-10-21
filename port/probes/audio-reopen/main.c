/* Silent bounded repeated SDL output open/close, using the paired HAL wrapper. */
#include <SDL.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <string.h>
void nxdk_audio_close_load_configuration(void);
void nxdk_audio_close_report_startup(void);
#ifndef PROBE_REOPEN_CYCLES
#define PROBE_REOPEN_CYCLES 20
#endif
#ifndef PROBE_WATCHDOG_MS
#define PROBE_WATCHDOG_MS 45000
#endif
#if PROBE_REOPEN_CYCLES < 1 || PROBE_REOPEN_CYCLES > 500 || PROBE_WATCHDOG_MS < 1000 || PROBE_WATCHDOG_MS > 300000
#error Invalid bounded probe configuration
#endif
static volatile LONG callbacks, finished;
static void SDLCALL fill(void *unused, Uint8 *stream, int len)
{
    (void)unused;memset(stream,0,len);InterlockedIncrement(&callbacks);
}
static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;Sleep(PROBE_WATCHDOG_MS);
    if(InterlockedCompareExchange(&finished,0,0))return 0;
    HANDLE f=CreateFileA("D:\\reopen-watchdog.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(f!=INVALID_HANDLE_VALUE) {
        const char message[]="reopen probe timeout; watchdog reboot\n";
        DWORD written;WriteFile(f,message,sizeof(message)-1,&written,NULL);
        IO_STATUS_BLOCK io;NtFlushBuffersFile(f,&io);CloseHandle(f);
    }
    XReboot();return 0;
}
int main(void)
{
    XVideoSetMode(640,480,16,REFRESH_DEFAULT);
    freopen("D:\\audio-reopen.log","w",stdout);setvbuf(stdout,NULL,_IONBF,0);
    freopen("D:\\audio-reopen-error.log","w",stderr);setvbuf(stderr,NULL,_IONBF,0);
    printf("Silent SDL reopen probe cycles=%d watchdog_ms=%d\n",PROBE_REOPEN_CYCLES,PROBE_WATCHDOG_MS);
    HANDLE guard=CreateThread(NULL,65536,watchdog,NULL,0,NULL);
    if(!guard){printf("result=inconclusive watchdog unavailable\n");XReboot();return 1;}
    CloseHandle(guard);nxdk_audio_close_load_configuration();
    if(SDL_Init(SDL_INIT_AUDIO)<0)goto failed;
    for(int cycle=1;cycle<=PROBE_REOPEN_CYCLES;++cycle) {
        SDL_AudioSpec want={0},have={0};want.freq=48000;want.format=AUDIO_S16SYS;
        want.channels=2;want.samples=1024;want.callback=fill;
        InterlockedExchange(&callbacks,0);
        printf("cycle=%d stage=open-enter\n",cycle);
        SDL_AudioDeviceID device=SDL_OpenAudioDevice(NULL,0,&want,&have,0);
        if(!device)goto failed;
        SDL_PauseAudioDevice(device,0);SDL_Delay(200);
        LONG before=InterlockedCompareExchange(&callbacks,0,0);
        SDL_Delay(200);LONG after=InterlockedCompareExchange(&callbacks,0,0);
        nxdk_audio_close_report_startup();
        printf("cycle=%d callbacks_before=%ld callbacks_after=%ld\n",cycle,before,after);
        printf("cycle=%d stage=close-enter\n",cycle);
        Uint32 began=SDL_GetTicks();SDL_CloseAudioDevice(device);
        MM_STATISTICS memory={0};memory.Length=sizeof(memory);
        const NTSTATUS memory_status=MmQueryStatistics(&memory);
        printf("cycle=%d stage=close-return elapsed_ms=%lu memory_status=%08lx available_pages=%lu\n",cycle,(unsigned long)(SDL_GetTicks()-began),(unsigned long)memory_status,(unsigned long)memory.AvailablePages);
        if(before<=0 || after<=before) {
            printf("result=%s stalled_cycle=%d\n",cycle==1?"inconclusive-initial-startup":"reopen-stalled",cycle);
            goto done;
        }
    }
    printf("result=%d-open-close-cycles-progressed\n",PROBE_REOPEN_CYCLES);
    goto done;
failed:
    printf("result=inconclusive SDL_error=%s\n",SDL_GetError());
done:
    SDL_Quit();InterlockedExchange(&finished,1);fflush(NULL);Sleep(250);XReboot();return 0;
}

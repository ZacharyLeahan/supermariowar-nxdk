// SPDX-License-Identifier: MIT
/* Paired close-path experiment for the pinned nxdk HAL. The selected SDK's
 * original driver is included unchanged. Only SDL's callback-NULL close call
 * is interposed; the normal output initialization and playback stay intact.
 * This is single-device, uniprocessor Xbox diagnostic code, not a new HAL API. */
#include <hal/audio.h>
#include <hal/xbox.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef NXDK_AUDIO_CLEAN_SHUTDOWN
static const int clean_close=1;
#else
static int clean_close;
#endif
static unsigned init_calls;
static unsigned startup_connected_before, startup_connected_after;
static unsigned last_connection_result, startup_connection_result;
void nxdk_audio_close_load_configuration(void)
{
#ifdef NXDK_AUDIO_CLEAN_SHUTDOWN
    // The game candidate always uses the qualified close path. No diagnostic
    // sidecar can silently select legacy shutdown in this build.
    printf("[audio-close] configuration=compiled mode=clean experimental=0\n");
#else
    char bytes[9];
    FILE *file=fopen("D:\\audio-close.txt","rb");
    const char *state="missing";
    clean_close=0;
    if(file) {
        const size_t n=fread(bytes,1,sizeof(bytes),file);
        const int error=ferror(file);fclose(file);
        if(!error && ((n==6 && !memcmp(bytes,"legacy",6)) || (n==7 && !memcmp(bytes,"legacy\n",7)))) state="valid";
        else if(!error && ((n==5 && !memcmp(bytes,"clean",5)) || (n==6 && !memcmp(bytes,"clean\n",6)))) {state="valid";clean_close=1;}
        else {state="invalid";fprintf(stderr,"[audio-close] invalid configuration; retaining legacy close\n");}
    }
    printf("[audio-close] configuration=%s mode=%s experimental=1\n",state,clean_close?"clean":"legacy");
#endif
#ifdef NXDK_AUDIO_CONFIGURATION_EXTRA
    NXDK_AUDIO_CONFIGURATION_EXTRA();
#endif
}

static BOOLEAN NTAPI capture_connection(PKINTERRUPT interrupt)
{
    const BOOLEAN result=KeConnectInterrupt(interrupt);
    last_connection_result=result;
    return result;
}
#define KeConnectInterrupt capture_connection
#define XAudioInit original_XAudioInit
#ifndef NXDK_AUDIO_HAL_SOURCE
#define NXDK_AUDIO_HAL_SOURCE <hal/audio.c>
#endif
#include NXDK_AUDIO_HAL_SOURCE
#undef XAudioInit
#undef KeConnectInterrupt

void nxdk_audio_close_report_startup(void)
{
    printf("[audio-close] startup init_calls=%u connected_before=%u connected_after=%u connect_return=%u\n",
           init_calls,startup_connected_before,startup_connected_after,startup_connection_result);
#ifdef NXDK_AUDIO_REPORT_EXTRA
    NXDK_AUDIO_REPORT_EXTRA();
#endif
}
static void close_failed(const char *reason)
{
    fprintf(stderr,"[audio-close] FAILED: %s; requesting full reboot before freeing DMA buffers\n",reason);
    fflush(NULL);
    FILE *marker=fopen("D:\\audio-close-failure.log","wb");
    if(marker) {fprintf(marker,"audio close failed: %s\n",reason);fclose(marker);}
    XReboot();
    for(;;) {LARGE_INTEGER delay;delay.QuadPart=-10000000;KeDelayExecutionThread(KernelMode,FALSE,&delay);}
}
static void clean_shutdown(void)
{
    if(!init_calls) {printf("[audio-close] clean no initialized device\n");return;}
    const unsigned connected=InterruptObject.Connected;
    printf("[audio-close] clean begin init_calls=%u connected=%u entry_irql=%u\n",init_calls,connected,(unsigned)KeGetCurrentIrql());
    volatile unsigned char *r=(volatile unsigned char *)ac97Device.mmio;
    // SDL has joined its audio thread. Block DPC execution, stop/mask DMA,
    // disconnect the owned ISR and remove queued completion work before free.
    const KIRQL old_irql=KeRaiseIrqlToDpcLevel();
    ac97Device.callback=NULL;ac97Device.callbackData=NULL;
    r[0x11b]=0;r[0x17b]=0;
    (void)r[0x11b];(void)r[0x17b];
    const BOOLEAN disconnected=!connected || KeDisconnectInterrupt(&InterruptObject);
    const BOOLEAN removed=KeRemoveQueueDpc(&DPCObject);
    KfLowerIrql(old_irql);
    if(!disconnected || InterruptObject.Connected) close_failed("interrupt disconnect unverified");
    // Reset only DMA engines; preserve the AC97 link/codec global control.
    r[0x11b]=2;r[0x17b]=2;
    const uint64_t frequency=KeQueryPerformanceFrequency();
    const uint64_t began=KeQueryPerformanceCounter();
    while((r[0x11b]&2) || (r[0x17b]&2)) {
        if(!frequency || (KeQueryPerformanceCounter()-began)*2>=frequency) close_failed("DMA reset exceeded 500 ms");
        KeStallExecutionProcessor(10);
    }
    r[0x116]=0xff;r[0x176]=0xff;
    ac97Device.mmio[0x110/4]=0;ac97Device.mmio[0x170/4]=0;
    (void)ac97Device.mmio[0x170/4];
    analogBufferCount=digitalBufferCount=0;
    analogDrained=digitalDrained=false;
    ac97Device.nextDescriptor=0;
    const unsigned released=init_calls;
    while(init_calls) {MmLockUnlockBufferPages(&ac97Device,sizeof(ac97Device),TRUE);--init_calls;}
    printf("[audio-close] clean complete irq_was_connected=%u disconnected=%u dpc_removed=%u locks_released=%u codec_reset=0 analog_control=%u digital_control=%u\n",
           connected,(unsigned)disconnected,(unsigned)removed,released,r[0x11b],r[0x17b]);
}
void XAudioInit(int bits,int channels,XAudioCallback callback,void *data)
{
    if(!callback && !data && clean_close) {clean_shutdown();return;}
    const unsigned before=InterruptObject.Connected;
    if(!callback) printf("[audio-close] legacy reinitialize init_calls=%u connected_before=%u\n",init_calls,before);
    original_XAudioInit(bits,channels,callback,data);++init_calls;
    if(callback) {startup_connected_before=before;startup_connected_after=InterruptObject.Connected;startup_connection_result=last_connection_result;}
    else printf("[audio-close] legacy complete init_calls=%u connected_after=%u connect_return=%u\n",init_calls,(unsigned)InterruptObject.Connected,last_connection_result);
}

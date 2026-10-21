/* Paired startup protocol experiment; teardown is always the clean path. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static int preserve_ready;
static unsigned cold_resets, preserved_links;
static uint32_t before_control, before_status, after_control, after_status;
static void load_init_protocol(void);
static void report_init_protocol(void);
#define NXDK_AUDIO_CLEAN_SHUTDOWN
#define NXDK_AUDIO_CONFIGURATION_EXTRA load_init_protocol
#define NXDK_AUDIO_REPORT_EXTRA report_init_protocol
#define NXDK_AUDIO_HAL_SOURCE "probes/audio-init-protocol/hal-audio.inc"
#include "../../AudioCloseExperiment.c"
static void load_init_protocol(void)
{
    char bytes[16];const char *state="missing";preserve_ready=0;
    FILE *file=fopen("D:\\audio-init.txt","rb");
    if(file) {
        size_t n=fread(bytes,1,sizeof(bytes),file);int error=ferror(file);fclose(file);
        if(!error && ((n==5 && !memcmp(bytes,"reset",5)) || (n==6 && !memcmp(bytes,"reset\n",6))))state="valid";
        else if(!error && ((n==14 && !memcmp(bytes,"preserve-ready",14)) || (n==15 && !memcmp(bytes,"preserve-ready\n",15)))) {state="valid";preserve_ready=1;}
        else {state="invalid";fprintf(stderr,"[audio-init-protocol] invalid setting; retaining cold reset\n");}
    }
    printf("[audio-init-protocol] configuration=%s requested=%s experimental=1\n",state,preserve_ready?"preserve-ready":"reset");
}
static void report_init_protocol(void)
{
    printf("[audio-init-protocol] requested=%s cold_resets=%u preserved_links=%u before_control=%08lx before_status=%08lx after_control=%08lx after_status=%08lx\n",
           preserve_ready?"preserve-ready":"reset",cold_resets,preserved_links,
           (unsigned long)before_control,(unsigned long)before_status,(unsigned long)after_control,(unsigned long)after_status);
}

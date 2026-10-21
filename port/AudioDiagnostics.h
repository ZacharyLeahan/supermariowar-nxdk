#pragma once
// Read-only snapshot; all three counters use the SDL device lock.
struct NxdkAudioStatus {
    unsigned long callbacks;
    unsigned long underruns;
    unsigned long loops;
};
NxdkAudioStatus nxdk_audio_status();

#include "AudioMixer.h"
#include "AudioDiagnostics.h"
#ifdef NXDK_STARTUP_WATCHDOG_MS
#include "Platform.h"
#endif
#include "sfx.h"
#include "util/Error.h"
#ifdef NXDK_AUDIO_PCI_LOG
#include "PciAudioState.h"
#endif
#include <SDL.h>
#if defined(NXDK_AUDIO_HW_DIAGNOSTICS) || defined(NXDK_AUDIO_FAILURE_DMA_LOG) || defined(NXDK_AUDIO_LINK_RECOVERY)
#include <hal/audio.h>
#include <xboxkrnl/xboxkrnl.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>
#define STB_VORBIS_HEADER_ONLY
#include "vendor/stb/stb_vorbis.c"
extern void SDLCALL musicfinished();
#ifdef NXDK_AUDIO_CLOSE_EXPERIMENT
extern "C" void nxdk_audio_close_report_startup(void);
#endif

bool fResumeMusic = false;
namespace
{
SDL_AudioDeviceID device = 0;
nxdk_audio::Mixer mixer;
int soundVolume = 128, musicVolume = 128;
unsigned long callbacks = 0, nonzero = 0;
#if defined(NXDK_AUDIO_HW_DIAGNOSTICS) || defined(NXDK_AUDIO_FAILURE_DMA_LOG) || defined(NXDK_AUDIO_LINK_RECOVERY)
extern "C" AC97_DEVICE ac97Device;
#endif
#ifdef NXDK_AUDIO_HW_DIAGNOSTICS
volatile LONG backendCallbacks = 0;
SDL_atomic_t callbackEntries = {0};
XAudioCallback originalBackendCallback = nullptr;
void trackBackendCallback(void *deviceState, void *data)
{
    // DPC context: integer atomics only, then the original SDL semaphore post.
    InterlockedIncrement(&backendCallbacks);
    originalBackendCallback(deviceState, data);
}
void observeBackendCallbacks()
{
    originalBackendCallback = ac97Device.callback;
    if (originalBackendCallback)
        ac97Device.callback = trackBackendCallback;
}
void logDmaStatus(const char *stage)
{
    const volatile auto *r = reinterpret_cast<const volatile uint8_t *>(0xfec00000);
    // Read-only layout observation for the pinned nxdk-sdl driver: two buffer
    // pointers, next_buffer, then playsem. Not a public SDL API or release path.
    static_assert(sizeof(void *) == 4);
    SDL_sem *semaphore = nullptr;
    if (ac97Device.callbackData)
        memcpy(&semaphore, static_cast<const uint8_t *>(ac97Device.callbackData) + 12, sizeof semaphore);
    printf("[audio-dma] %s backend_callbacks=%ld callback_entries=%d semaphore=%lu "
           "sdl_status=%d analog civ=%u lvi=%u status=%u control=%u "
           "digital civ=%u lvi=%u status=%u control=%u\n", stage,
           InterlockedCompareExchange(&backendCallbacks, 0, 0), SDL_AtomicGet(&callbackEntries),
           (unsigned long)(semaphore ? SDL_SemValue(semaphore) : 0),
           (int)SDL_GetAudioDeviceStatus(device),
           r[0x114], r[0x115], r[0x116], r[0x11b], r[0x174], r[0x175], r[0x176], r[0x17b]);
}
#endif
#ifdef NXDK_AUDIO_FAILURE_DMA_LOG
void logFailedStartupDma(unsigned sample)
{
#ifdef NXDK_AUDIO_PCI_LOG
    if (sample == 0) {
#ifdef NXDK_STARTUP_WATCHDOG_MS
        nxdk_startup_checkpoint(NxdkStartupStage::pci);
#endif
        nxdk_log_pci_audio("failed-startup");
#ifdef NXDK_STARTUP_WATCHDOG_MS
        nxdk_startup_checkpoint(NxdkStartupStage::audio);
#endif
    }
#endif
    const volatile auto *r = reinterpret_cast<const volatile uint8_t *>(0xfec00000);
    printf("[audio-dma-failure] sample=%u sdl_status=%d analog civ=%u lvi=%u status=%u "
           "control=%u digital civ=%u lvi=%u status=%u control=%u\n", sample,
           (int)SDL_GetAudioDeviceStatus(device), r[0x114], r[0x115], r[0x116], r[0x11b],
           r[0x174], r[0x175], r[0x176], r[0x17b]);
    const auto *r16 = reinterpret_cast<const volatile uint16_t *>(r);
    const auto *r32 = reinterpret_cast<const volatile uint32_t *>(r);
    printf("[audio-platform-read] sample=%u reading_mmio=17c\n", sample);
    printf("[audio-dma-registers] sample=%u global_control=%08lx global_status=%08lx platform_17c=%08lx "
           "analog base=%08lx remaining=%u prefetched=%u "
           "digital base=%08lx remaining=%u prefetched=%u\n", sample,
           (unsigned long)r32[0x12c / 4], (unsigned long)r32[0x130 / 4], (unsigned long)r32[0x17c / 4],
           (unsigned long)r32[0x110 / 4], r16[0x118 / 2], r[0x11a],
           (unsigned long)r32[0x170 / 4], r16[0x178 / 2], r[0x17a]);
    // These pinned HAL objects were locked by XAudioInit. Observe only; do not
    // submit samples, alter callbacks, clear status or restart the engines.
    printf("[audio-dma-descriptors] sample=%u next=%u expected_analog_base=%08lx "
           "expected_digital_base=%08lx\n", sample, ac97Device.nextDescriptor,
           (unsigned long)MmGetPhysicalAddress((void *)&ac97Device.pcmOutDescriptor[0]),
           (unsigned long)MmGetPhysicalAddress((void *)&ac97Device.pcmSpdifDescriptor[0]));
    for (unsigned i = 0; i < 2; ++i) {
        const volatile auto &analog = ac97Device.pcmOutDescriptor[i];
        const volatile auto &digital = ac97Device.pcmSpdifDescriptor[i];
        printf("[audio-dma-buffer] sample=%u index=%u analog address=%08lx samples=%u "
               "control=%u digital address=%08lx samples=%u control=%u\n", sample, i,
               (unsigned long)analog.bufferStartAddress, analog.bufferLengthInSamples,
               analog.bufferControl, (unsigned long)digital.bufferStartAddress,
               digital.bufferLengthInSamples, digital.bufferControl);
    }
}
#endif
class AudioLock
{
  public:
    AudioLock()
    {
        if (device)
            SDL_LockAudioDevice(device);
    }
    ~AudioLock()
    {
        if (device)
            SDL_UnlockAudioDevice(device);
    }
};
#ifdef NXDK_AUDIO_LINK_RECOVERY
void tryStartupDmaRestart(unsigned long stagnantCount)
{
    auto *r = reinterpret_cast<volatile uint32_t *>(0xfec00000);
    unsigned long before;
    { AudioLock lock; before = callbacks; }
    const uint32_t control = r[0x12c / 4];
    const uint32_t status = r[0x130 / 4];
    if (before != stagnantCount || (control & 4)) {
        printf("[audio-link-experiment] skipped callbacks=%lu control=%08lx status=%08lx\n",
               before, (unsigned long)control, (unsigned long)status);
        return;
    }
    const auto position = [r]() {
        const auto *r8 = reinterpret_cast<const volatile uint8_t *>(r);
        const auto *r16 = reinterpret_cast<const volatile uint16_t *>(r);
        return std::array<uint32_t, 2>{
            (uint32_t(r8[0x114]) << 16) | r16[0x118 / 2],
            (uint32_t(r8[0x174]) << 16) | r16[0x178 / 2]};
    };
    const auto firstPosition = position();
    SDL_Delay(50);
    const auto secondPosition = position();
    { AudioLock lock; before = callbacks; }
    const auto *r8 = reinterpret_cast<const volatile uint8_t *>(r);
    const bool running = (r8[0x11b] & 1) && (r8[0x17b] & 1);
    if (before != stagnantCount || firstPosition != secondPosition || !running) {
        printf("[audio-link-experiment] skipped progress_or_pause callbacks=%lu "
               "running=%d analog=%08lx/%08lx digital=%08lx/%08lx\n", before, running,
               (unsigned long)firstPosition[0], (unsigned long)secondPosition[0],
               (unsigned long)firstPosition[1], (unsigned long)secondPosition[1]);
        return;
    }
    const uint32_t analogBase = r[0x110 / 4], digitalBase = r[0x170 / 4];
    bool initialQueue = before == 0 && stagnantCount == 0 &&
        r8[0x114] == 0 && r8[0x174] == 0 && r8[0x115] == 1 && r8[0x175] == 1 &&
        r8[0x116] == 0 && r8[0x176] == 0 && ac97Device.nextDescriptor == 2 &&
        analogBase == MmGetPhysicalAddress((void *)&ac97Device.pcmOutDescriptor[0]) &&
        digitalBase == MmGetPhysicalAddress((void *)&ac97Device.pcmSpdifDescriptor[0]);
    for (unsigned i = 0; i < 2; ++i) {
        const volatile auto &analog = ac97Device.pcmOutDescriptor[i];
        const volatile auto &digital = ac97Device.pcmSpdifDescriptor[i];
        initialQueue &= analog.bufferStartAddress != 0 &&
            analog.bufferStartAddress == digital.bufferStartAddress &&
            analog.bufferLengthInSamples == 2048 && digital.bufferLengthInSamples == 2048 &&
            analog.bufferControl == 0x8000 && digital.bufferControl == 0x8000;
    }
    if (!initialQueue) {
        printf("[audio-link-experiment] protocol=v3 skipped initial_queue_guard callbacks=%lu next=%u\n",
               before, ac97Device.nextDescriptor);
        return;
    }
    printf("[audio-link-experiment] protocol=v3 dma_restart frozen_ms=50 analog_position=%08lx "
           "digital_position=%08lx control=%08lx status=%08lx\n", (unsigned long)secondPosition[0],
           (unsigned long)secondPosition[1], (unsigned long)control, (unsigned long)status);
    // Restart only the untouched two-buffer startup queue. Keep HAL buffer
    // counts, nextDescriptor, callback and SDL semaphore unchanged. No codec
    // reset, new samples or synthetic completion is issued.
    auto *w8 = reinterpret_cast<volatile uint8_t *>(r);
    XAudioPause();
    w8[0x11b] = 0x1e; // HAL's analog bus-master-reset sequence, RUN clear.
    w8[0x17b] = 0x1e;
    const Uint32 started = SDL_GetTicks();
    while ((w8[0x11b] & 2) || (w8[0x17b] & 2)) {
        if (SDL_GetTicks() - started >= 500) {
            XAudioPause();
            printf("[audio-link-experiment] protocol=v3 reset_timeout elapsed_ms=%lu\n",
                   (unsigned long)(SDL_GetTicks() - started));
            return;
        }
        SDL_Delay(1);
    }
    w8[0x116] = 0xff;
    w8[0x176] = 0xff;
    r[0x110 / 4] = analogBase;
    r[0x170 / 4] = digitalBase;
    w8[0x115] = 1;
    w8[0x175] = 1;
    XAudioPlay();
    SDL_Delay(100);
    unsigned long after;
    { AudioLock lock; after = callbacks; }
    const auto finalPosition = position();
    printf("[audio-link-experiment] protocol=v3 end elapsed_ms=%lu callbacks=%lu "
           "next=%u control=%08lx status=%08lx analog_position=%08lx digital_position=%08lx\n",
           (unsigned long)(SDL_GetTicks() - started), after, ac97Device.nextDescriptor,
           (unsigned long)r[0x12c / 4], (unsigned long)r[0x130 / 4],
           (unsigned long)finalPosition[0], (unsigned long)finalPosition[1]);
}
#endif
constexpr size_t ringSize = 48000 * 2;
std::array<int16_t, ringSize> musicRing{};
size_t ringRead = 0, ringWrite = 0, ringCount = 0;
std::string requestedPath;
Uint32 musicGeneration = 0;
bool musicActive = false, musicPaused = false, musicReady = false, musicEof = false, musicLoop = false,
     musicFinishedPending = false;
SDL_atomic_t workerQuit = {0};
SDL_Thread *musicThread = nullptr;
unsigned long musicSamples = 0, underruns = 0, decodedFrames = 0, musicLoops = 0;

// Called with the SDL audio device locked (as is the audio callback itself).
void mixMusic(int16_t *out, size_t count)
{
    if (!musicActive || musicPaused || !musicReady)
        return;
    const size_t n = std::min(count, ringCount);
    for (size_t i = 0; i < n; i++) {
        out[i] = musicRing[ringRead] * musicVolume / 128;
        ringRead = (ringRead + 1) % ringSize;
    }
    ringCount -= n;
    musicSamples += (unsigned long)n;
    if (n < count && !musicEof)
        ++underruns;
    if (!ringCount && musicEof) {
        musicActive = false;
        musicFinishedPending = true;
    }
}
bool pushPcm(const int16_t *data, size_t count, Uint32 generation)
{
    size_t at = 0;
    while (at < count && !SDL_AtomicGet(&workerQuit)) {
        size_t n = 0;
        {
            AudioLock lock;
            if (generation != musicGeneration || !musicActive)
                return false;
            n = std::min(count - at, ringSize - ringCount);
            for (size_t i = 0; i < n; i++) {
                musicRing[ringWrite] = data[at + i];
                ringWrite = (ringWrite + 1) % ringSize;
            }
            ringCount += n;
            if (ringCount >= 8192)
                musicReady = true;
        }
        at += n;
        if (!n)
            SDL_Delay(2);
    }
    return at == count;
}
int SDLCALL musicWorker(void *)
{
    stb_vorbis *decoder = nullptr;
    SDL_AudioStream *conversion = nullptr;
    std::vector<unsigned char> compressed;
    Uint32 generation = 0;
    bool loop = false, decodedAny = false;
    std::array<int16_t, 8192> decoded{}, converted{};
    while (!SDL_AtomicGet(&workerQuit)) {
        bool changed = false, active = false;
        std::string path;
        {
            AudioLock lock;
            changed = generation != musicGeneration;
            active = musicActive;
            if (changed) {
                generation = musicGeneration;
                path = requestedPath;
                loop = musicLoop;
            }
        }
        if (changed) {
            if (decoder)
                stb_vorbis_close(decoder);
            decoder = nullptr;
            if (conversion)
                SDL_FreeAudioStream(conversion);
            conversion = nullptr;
            decodedAny = false;
            if (active) {
                int error = 0;
                // Avoid stb's per-byte fgetc/stdio locking on the Xbox. Keep
                // only the compressed song in memory; decoded PCM stays bounded.
                compressed.clear();
                FILE *file = fopen(path.c_str(), "rb");
                if (file) {
                    if (fseek(file, 0, SEEK_END) == 0) {
                        const long bytes = ftell(file);
                        if (bytes > 0 && bytes <= 4 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0) {
                            compressed.resize((size_t)bytes);
                            if (fread(compressed.data(), 1, compressed.size(), file) != compressed.size())
                                compressed.clear();
                        }
                    }
                    fclose(file);
                }
                if (!compressed.empty())
                    decoder =
                        stb_vorbis_open_memory(compressed.data(), (int)compressed.size(), &error, nullptr);
                if (decoder) {
                    const auto info = stb_vorbis_get_info(decoder);
                    conversion =
                        SDL_NewAudioStream(AUDIO_S16SYS, 2, info.sample_rate, AUDIO_S16SYS, 2, 48000);
                    printf("[audio] Ogg memory stream %s rate=%u channels=%d generation=%lu "
                           "compressed_bytes=%lu\n",
                           path.c_str(), info.sample_rate, info.channels, (unsigned long)generation,
                           (unsigned long)compressed.size());
                }
                if (!decoder || !conversion) {
                    fprintf(stderr, "[audio] Cannot stream %s vorbis_error=%d SDL=%s\n", path.c_str(), error,
                            SDL_GetError());
                    AudioLock lock;
                    if (generation == musicGeneration) {
                        musicActive = false;
                        musicFinishedPending = true;
                    }
                }
            }
        }
        if (!active || !decoder || !conversion) {
            SDL_Delay(4);
            continue;
        }
        bool room;
        {
            AudioLock lock;
            room = ringSize - ringCount >= 8192;
        }
        if (!room) {
            SDL_Delay(2);
            continue;
        }
        const int frames =
            stb_vorbis_get_samples_short_interleaved(decoder, 2, decoded.data(), decoded.size());
        bool rewound = false;
        if (frames > 0) {
            decodedAny = true;
            {
                AudioLock lock;
                decodedFrames += (unsigned long)frames;
            }
            if (SDL_AudioStreamPut(conversion, decoded.data(), frames * 4) < 0) {
                fprintf(stderr, "[audio] resampler put: %s\n", SDL_GetError());
                AudioLock lock;
                if (generation == musicGeneration)
                    musicActive = false;
                continue;
            }
        } else {
            // A repeat is one continuous PCM stream. Keep the resampler's
            // history; flushing/clearing each lap inserts end/start padding.
            rewound = loop && decodedAny && stb_vorbis_seek_start(decoder);
            if (!rewound)
                SDL_AudioStreamFlush(conversion);
        }
        bool current = true;
        while (SDL_AudioStreamAvailable(conversion) > 0) {
            const int bytes = SDL_AudioStreamGet(conversion, converted.data(), converted.size() * 2);
            if (bytes <= 0)
                break;
            if (!pushPcm(converted.data(), bytes / 2, generation)) {
                current = false;
                break;
            }
        }
        if (!current)
            continue;
        if (!frames) {
            if (rewound) {
                decodedAny = false;
                AudioLock lock;
                ++musicLoops;
            } else {
                {
                    AudioLock lock;
                    if (generation == musicGeneration) {
                        musicEof = true;
                        musicReady = true;
                    }
                }
                stb_vorbis_close(decoder);
                decoder = nullptr;
                SDL_FreeAudioStream(conversion);
                conversion = nullptr;
            }
        }
    }
    if (decoder)
        stb_vorbis_close(decoder);
    if (conversion)
        SDL_FreeAudioStream(conversion);
    return 0;
}

void SDLCALL callback(void *, Uint8 *bytes, int len)
{
#ifdef NXDK_AUDIO_HW_DIAGNOSTICS
    SDL_AtomicAdd(&callbackEntries, 1);
#endif
    // nxdk returns write-combining DMA memory here. Read/modify/write mixing
    // against that buffer is expensive; keep all reads in cached stack memory
    // and write each finished block to the device once.
    std::array<int16_t, 2048> mixed;
    const size_t samples = (size_t)len / sizeof(int16_t);
    bool audible = false;
    for (size_t base = 0; base < samples; base += mixed.size()) {
        const size_t count = std::min(mixed.size(), samples - base);
        std::fill_n(mixed.data(), count, int16_t(0));
        mixMusic(mixed.data(), count);
        mixer.mix(mixed.data(), count, soundVolume);
        if (!audible)
            audible = std::any_of(mixed.begin(), mixed.begin() + count, [](int16_t n) { return n != 0; });
        memcpy(bytes + base * sizeof(int16_t), mixed.data(), count * sizeof(int16_t));
    }
    if (len & 1)
        bytes[len - 1] = 0;
    ++callbacks;
    if (audible)
        ++nonzero;
}
} // namespace
NxdkAudioStatus nxdk_audio_status()
{
    AudioLock lock;
    return {callbacks, underruns, musicLoops};
}
void nxdk_audio_update()
{
    bool finished = false;
    {
        AudioLock lock;
        finished = musicFinishedPending;
        musicFinishedPending = false;
    }
    if (finished)
        musicfinished();
    static Uint32 last = 0;
    if (!device || SDL_GetTicks() - last < 10000)
        return;
    unsigned long c, n, s, done;
    {
        AudioLock lock;
        c = callbacks;
        n = nonzero;
        s = mixer.starts;
        done = mixer.completed;
    }
    printf("[audio] callbacks=%lu nonzero_buffers=%lu effect_starts=%lu completed=%lu\n", c, n, s, done);
    unsigned long played, decoded, gaps, loops, buffered;
    bool active, paused;
    {
        AudioLock lock;
        played = musicSamples;
        decoded = decodedFrames;
        gaps = underruns;
        loops = musicLoops;
        buffered = (unsigned long)ringCount;
        active = musicActive;
        paused = musicPaused;
    }
    printf("[audio] music_samples=%lu decoded_frames=%lu underruns=%lu loops=%lu buffered_samples=%lu "
           "active=%d paused=%d\n",
           played, decoded, gaps, loops, buffered, active, paused);
    last = SDL_GetTicks();
#ifdef NXDK_AUDIO_HW_DIAGNOSTICS
    logDmaStatus("running");
#endif
}
void MixDeleter::operator()(Mix_Chunk *p) const noexcept
{
    {
        AudioLock lock;
        mixer.stop(p ? &p->pcm : nullptr);
    }
    delete p;
}
void MixDeleter::operator()(Mix_Music *p) const noexcept { delete p; }
#ifdef NXDK_AUDIO_INIT_STATE_LOG
// Same read-only MMIO fields as the native silent-startup probe. No PCI scan,
// descriptor dereference, callback replacement, status clearing or recovery.
static void logAudioInitState(const char *phase)
{
    const volatile auto *r = reinterpret_cast<const volatile uint8_t *>(0xfec00000);
    const volatile auto *r16 = reinterpret_cast<const volatile uint16_t *>(r);
    const volatile auto *r32 = reinterpret_cast<const volatile uint32_t *>(r);
    printf("[audio-platform-read] phase=%s reading_mmio=17c\n", phase);
    printf("[audio-init-state] phase=%s global_control=%08lx global_status=%08lx platform_17c=%08lx "
           "analog base=%08lx civ=%u lvi=%u status=%u control=%u remaining=%u "
           "digital base=%08lx civ=%u lvi=%u status=%u control=%u remaining=%u\n", phase,
           (unsigned long)r32[0x12c / 4], (unsigned long)r32[0x130 / 4], (unsigned long)r32[0x17c / 4],
           (unsigned long)r32[0x110 / 4], r[0x114], r[0x115], r[0x116], r[0x11b], r16[0x118 / 2],
           (unsigned long)r32[0x170 / 4], r[0x174], r[0x175], r[0x176], r[0x17b], r16[0x178 / 2]);
}
#endif
bool sfx_init()
{
#ifdef NXDK_AUDIO_INIT_STATE_LOG
    logAudioInitState("before-sdl-audio");
#endif
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[audio] init: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want = {}, have = {};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
#ifdef NXDK_AUDIO_INIT_STATE_LOG
    logAudioInitState("after-open");
#endif
    if (!device) {
        fprintf(stderr, "[audio] open: %s\n", SDL_GetError());
        return false;
    }
#ifdef NXDK_AUDIO_HW_DIAGNOSTICS
    observeBackendCallbacks();
#endif
    printf("[audio] SDL driver=%s rate=%d channels=%d format=%u samples=%u\n", SDL_GetCurrentAudioDriver(),
           have.freq, have.channels, have.format, have.samples);
    // Start output before allocating/starting the decoder worker.
    SDL_PauseAudioDevice(device, 0);
    SDL_Delay(100);
    unsigned long startupCallbacks;
    {
        AudioLock lock;
        startupCallbacks = callbacks;
    }
    printf("[audio] startup callbacks=%lu\n", startupCallbacks);
#ifdef NXDK_AUDIO_CLOSE_EXPERIMENT
    nxdk_audio_close_report_startup();
#endif
#ifdef NXDK_AUDIO_INIT_STATE_LOG
    logAudioInitState("after-startup-window");
#endif
#ifdef NXDK_AUDIO_FAILURE_DMA_LOG
    // This variant does not replace the HAL callback or change a DMA control
    // register. Observe only an already-failed startup, retaining its timing.
    if (!startupCallbacks) {
        for (unsigned sample = 0; sample < 4; ++sample) {
            logFailedStartupDma(sample);
            SDL_Delay(100);
        }
        fprintf(stderr, "[audio] Output callback stalled at startup; DMA failure samples recorded\n");
    }
#endif
#ifdef NXDK_AUDIO_HW_DIAGNOSTICS
    logDmaStatus("startup");
    if (!startupCallbacks)
        fprintf(stderr, "[audio] Output callback stalled at startup; audio acceptance failed\n");
#endif
#ifdef NXDK_AUDIO_LINK_RECOVERY
    SDL_Delay(100);
    unsigned long laterCallbacks;
    { AudioLock lock; laterCallbacks = callbacks; }
    printf("[audio-link-experiment] progress_window before=%lu after=%lu\n",
           startupCallbacks, laterCallbacks);
    if (laterCallbacks == startupCallbacks) {
#ifdef NXDK_AUDIO_FAILURE_DMA_LOG
        if (startupCallbacks) {
            for (unsigned sample = 0; sample < 4; ++sample) {
                logFailedStartupDma(sample);
                SDL_Delay(100);
            }
            fprintf(stderr, "[audio] Output stopped after initial startup callbacks\n");
        }
#endif
        tryStartupDmaRestart(laterCallbacks);
    }
#endif
    SDL_AtomicSet(&workerQuit, 0);
    // The decoder uses 32 KiB of local PCM buffers plus codec stack space.
    musicThread = SDL_CreateThreadWithStackSize(musicWorker, "smw-music", 256 * 1024, nullptr);
    if (!musicThread) {
        fprintf(stderr, "[audio] music thread: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(device);
        device = 0;
        return false;
    }
    return true;
}
void sfx_close()
{
    SDL_AtomicSet(&workerQuit, 1);
    if (musicThread) {
        SDL_WaitThread(musicThread, nullptr);
        musicThread = nullptr;
    }
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}
void sfx_stopallsounds()
{
    AudioLock lock;
    mixer.stop();
}
void sfx_setmusicvolume(int n)
{
    AudioLock lock;
    musicVolume = std::clamp(n, 0, 128);
}
void sfx_setsoundvolume(int n)
{
    AudioLock lock;
    soundVolume = std::clamp(n, 0, 128);
}
bool sfx_canPlayAudio() { return device != 0; }
sfxSound::sfxSound(const smw::filesystem::path &path)
{
    const auto name = path.string();
    SDL_AudioSpec spec = {};
    Uint8 *raw = nullptr;
    Uint32 size = 0;
    if (!SDL_LoadWAV(name.c_str(), &spec, &raw, &size))
        SMW_RAISE("Couldn't load WAV: " + name + ": " + SDL_GetError());
    SDL_AudioCVT cvt = {};
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, 48000) < 0) {
        SDL_FreeWAV(raw);
        SMW_RAISE("Couldn't convert WAV: " + name);
    }
    cvt.len = (int)size;
    cvt.buf = (Uint8 *)SDL_malloc(size * cvt.len_mult);
    if (!cvt.buf) {
        SDL_FreeWAV(raw);
        SMW_RAISE("Out of memory converting WAV: " + name);
    }
    memcpy(cvt.buf, raw, size);
    SDL_FreeWAV(raw);
    if (SDL_ConvertAudio(&cvt) < 0) {
        SDL_free(cvt.buf);
        SMW_RAISE("WAV conversion failed: " + name);
    }
    m_sfx = MixChunkPtr(new Mix_Chunk());
    const int bytes = cvt.needed ? cvt.len_cvt : cvt.len;
    m_sfx->pcm.resize(bytes / 2);
    memcpy(m_sfx->pcm.data(), cvt.buf, bytes);
    SDL_free(cvt.buf);
    printf("[audio] WAV %s pcm_bytes=%d\n", name.c_str(), bytes);
}
bool sfxSound::play()
{
    if (!device || !m_sfx)
        return false;
    const Uint32 now = SDL_GetTicks();
    if (now - m_last_start_time < 40)
        return false;
    AudioLock lock;
    if (mixer.play(&m_sfx->pcm, 0) < 0)
        return false;
    m_last_start_time = now;
    return true;
}
void sfxSound::playLoop(int n)
{
    if (device && m_sfx) {
        AudioLock lock;
        mixer.play(&m_sfx->pcm, n);
    }
}
void sfxSound::stop()
{
    if (m_sfx) {
        AudioLock lock;
        mixer.stop(&m_sfx->pcm);
    }
}
bool sfxSound::isPlaying() const
{
    AudioLock lock;
    return m_sfx && mixer.playing(&m_sfx->pcm);
}
void sfxSound::onChannelFinished(int) {}
sfxMusic::sfxMusic(const smw::filesystem::path &path) { m_music = MixMusicPtr(new Mix_Music{path.string()}); }
void sfxMusic::play(bool once, bool resume)
{
    if (!device || !m_music)
        return;
    AudioLock lock;
    requestedPath = m_music->path;
    ++musicGeneration;
    ringRead = ringWrite = ringCount = 0;
    musicActive = true;
    musicPaused = false;
    musicReady = musicEof = musicFinishedPending = false;
    musicLoop = !once;
    fResumeMusic = resume;
}
void sfxMusic::stop()
{
    AudioLock lock;
    ++musicGeneration;
    musicActive = musicReady = musicEof = musicFinishedPending = false;
    ringCount = 0;
}
void sfxMusic::togglePause()
{
    AudioLock lock;
    if (m_music && requestedPath == m_music->path) {
        musicPaused = !musicPaused;
        m_paused = musicPaused;
    }
}
bool sfxMusic::isPlaying() const
{
    AudioLock lock;
    return m_music && musicActive && requestedPath == m_music->path;
}

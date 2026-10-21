#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <hal/video.h>
#include <hal/xbox.h>
#include <vector>
#include <windows.h>
#define STB_VORBIS_HEADER_ONLY
#include "../../vendor/stb/stb_vorbis.c"

static unsigned long milliseconds(Uint64 ticks)
{
    return (unsigned long)(ticks * 1000 / SDL_GetPerformanceFrequency());
}
static int SDLCALL benchmark(void *)
{
    const char *tracks[] = {"menu.ogg", "overground.ogg", "stageclear.ogg"};
    unsigned failures = 0;
    std::array<short, 8192> pcm{}, converted{};
    for (const auto *track : tracks) {
        char path[256];
        snprintf(path, sizeof path, "D:\\%s", track);
        const Uint64 readStart = SDL_GetPerformanceCounter();
        FILE *file = fopen(path, "rb");
        if (!file) {
            printf("FAIL open %s\n", path);
            ++failures;
            continue;
        }
        bool valid = fseek(file, 0, SEEK_END) == 0;
        const long bytes = valid ? ftell(file) : 0;
        valid = valid && bytes > 0 && bytes <= 4 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0;
        std::vector<unsigned char> compressed(valid ? size_t(bytes) : 0);
        if (valid)
            valid = fread(compressed.data(), 1, compressed.size(), file) == compressed.size();
        fclose(file);
        if (!valid) {
            printf("FAIL read %s\n", path);
            ++failures;
            continue;
        }
        printf("track=%s compressed_bytes=%ld read_ms=%lu\n", track, bytes,
               milliseconds(SDL_GetPerformanceCounter() - readStart));
        for (unsigned mode = 0; mode < 2; ++mode) {
            int error = 0;
            stb_vorbis *decoder =
                stb_vorbis_open_memory(compressed.data(), int(compressed.size()), &error, nullptr);
            if (!decoder) {
                printf("FAIL decoder error=%d\n", error);
                ++failures;
                continue;
            }
            const auto info = stb_vorbis_get_info(decoder);
            SDL_AudioStream *stream =
                mode ? SDL_NewAudioStream(AUDIO_S16SYS, 2, info.sample_rate, AUDIO_S16SYS, 2, 48000)
                     : nullptr;
            if (mode && !stream) {
                printf("FAIL resampler %s\n", SDL_GetError());
                stb_vorbis_close(decoder);
                ++failures;
                continue;
            }
            const unsigned long limit = std::min<unsigned long>(
                info.sample_rate * 15ul, stb_vorbis_stream_length_in_samples(decoder));
            Uint64 decodeTicks = 0, resampleTicks = 0;
            unsigned long frames = 0, outputFrames = 0;
            const Uint64 start = SDL_GetPerformanceCounter();
            bool ok = true;
            while (frames < limit) {
                const Uint64 before = SDL_GetPerformanceCounter();
                const int n =
                    stb_vorbis_get_samples_short_interleaved(decoder, 2, pcm.data(), pcm.size());
                decodeTicks += SDL_GetPerformanceCounter() - before;
                if (!n)
                    break;
                frames += (unsigned long)n;
                if (stream) {
                    const Uint64 convertStart = SDL_GetPerformanceCounter();
                    ok = SDL_AudioStreamPut(stream, pcm.data(), n * 4) == 0;
                    while (ok && SDL_AudioStreamAvailable(stream) > 0) {
                        const int got =
                            SDL_AudioStreamGet(stream, converted.data(), converted.size() * 2);
                        if (got <= 0) {
                            ok = false;
                            break;
                        }
                        outputFrames += (unsigned long)got / 4;
                    }
                    resampleTicks += SDL_GetPerformanceCounter() - convertStart;
                    if (!ok)
                        break;
                }
            }
            if (stream && ok) {
                const Uint64 convertStart = SDL_GetPerformanceCounter();
                ok = SDL_AudioStreamFlush(stream) == 0;
                while (ok && SDL_AudioStreamAvailable(stream) > 0) {
                    const int got =
                        SDL_AudioStreamGet(stream, converted.data(), converted.size() * 2);
                    if (got <= 0) {
                        ok = false;
                        break;
                    }
                    outputFrames += (unsigned long)got / 4;
                }
                resampleTicks += SDL_GetPerformanceCounter() - convertStart;
            }
            ok = ok && frames > 0 && limit > 0 && frames >= limit &&
                 stb_vorbis_get_error(decoder) == VORBIS__no_error;
            printf("mode=%s rate=%u source_frames=%lu source_ms=%lu decode_ms=%lu resample_ms=%lu "
                   "wall_ms=%lu output_frames=%lu ok=%d\n",
                   mode ? "decode-resample" : "decode-only", info.sample_rate, frames,
                   frames * 1000 / info.sample_rate, milliseconds(decodeTicks),
                   milliseconds(resampleTicks), milliseconds(SDL_GetPerformanceCounter() - start),
                   outputFrames, ok);
            if (!ok)
                ++failures;
            if (stream)
                SDL_FreeAudioStream(stream);
            stb_vorbis_close(decoder);
        }
    }
    printf("result=%s failures=%u\n", failures ? "failed" : "passed", failures);
    return int(failures);
}
int main()
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);
    freopen("D:\\audio-cost.log", "w", stdout);
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("Silent audio-cost probe: first 15 seconds per track; no device opened, no game "
           "rendering\n");
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
        printf("FAIL SDL init %s\n", SDL_GetError());
    } else {
        SDL_Thread *worker =
            SDL_CreateThreadWithStackSize(benchmark, "audio-cost", 256 * 1024, nullptr);
        if (worker)
            SDL_WaitThread(worker, nullptr);
        else
            printf("FAIL thread %s\n", SDL_GetError());
    }
    fflush(stdout);
    Sleep(500);
    XReboot();
    return 0;
}

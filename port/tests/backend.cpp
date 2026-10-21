#include "Audio.cpp"
#include <cassert>
#include <cstdlib>
namespace smw
{
[[noreturn]] void fatal(const char *s)
{
    fprintf(stderr, "FATAL %s\n", s);
    abort();
}
} // namespace smw
unsigned finishes = 0;
void SDLCALL musicfinished() { ++finishes; }
// Compare the production worker across a loop boundary with one uninterrupted
// resampler fed the same source chunks. Flushing per loop must not add padding.
static void checkLoopContinuity(sfxMusic &track)
{
    FILE *file = fopen("stageclear.ogg", "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0 && fseek(file, 0, SEEK_SET) == 0);
    std::vector<unsigned char> compressed(length);
    assert(fread(compressed.data(), 1, compressed.size(), file) == compressed.size());
    fclose(file);
    int error = 0;
    auto *source = stb_vorbis_open_memory(compressed.data(), compressed.size(), &error, nullptr);
    assert(source);
    const auto info = stb_vorbis_get_info(source);
    auto *stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, info.sample_rate, AUDIO_S16SYS, 2, 48000);
    assert(stream);
    std::array<int16_t, 8192> decoded{}, converted{};
    std::vector<int16_t> reference;
    for (int lap = 0; lap < 3; ++lap) {
        for (;;) {
            const int frames = stb_vorbis_get_samples_short_interleaved(source, 2, decoded.data(), decoded.size());
            if (!frames) break;
            assert(SDL_AudioStreamPut(stream, decoded.data(), frames * 4) == 0);
            while (SDL_AudioStreamAvailable(stream) > 0) {
                const int bytes = SDL_AudioStreamGet(stream, converted.data(), converted.size() * 2);
                assert(bytes > 0);
                reference.insert(reference.end(), converted.begin(), converted.begin() + bytes / 2);
            }
        }
        assert(stb_vorbis_seek_start(source));
    }
    SDL_FreeAudioStream(stream);
    stb_vorbis_close(source);
    // Three reference laps supply enough data to compare about 1.5 laps,
    // safely beyond the first seam and before final-stream padding.
    const size_t wanted = reference.size() / 2;
    assert(wanted > 48000 * 2);
    SDL_PauseAudioDevice(device, 1);
    track.play(false, false);
    std::vector<int16_t> actual;
    std::array<int16_t, 2048> block{};
    const Uint32 started = SDL_GetTicks();
    while (actual.size() < wanted && SDL_GetTicks() - started < 10000) {
        bool consumed = false;
        {
            AudioLock lock;
            const size_t count = std::min(block.size(), wanted - actual.size());
            if (musicReady && ringCount >= count) {
                callback(nullptr, reinterpret_cast<Uint8 *>(block.data()), count * 2);
                actual.insert(actual.end(), block.begin(), block.begin() + count);
                consumed = true;
            }
        }
        if (!consumed) SDL_Delay(1);
    }
    assert(actual.size() == wanted);
    for (size_t i = 0; i < wanted; ++i) {
        if (actual[i] != reference[i]) {
            fprintf(stderr, "Loop PCM mismatch at sample %zu: worker=%d continuous=%d\n", i, actual[i], reference[i]);
            abort();
        }
    }
    track.stop();
    SDL_PauseAudioDevice(device, 0);
    printf("PASS: production worker loop matches uninterrupted resampling through first seam\n");
}
int main()
{
    // Compare a callback larger than the scratch block against known PCM,
    // including its tail and byte guards. This also exercises the DMA-copy path.
    {
        nxdk_audio::Samples wave{30000, -30000, 1000, -1000};
        mixer.play(&wave, -1);
        soundVolume = 64;
        std::array<Uint8, 8207> bytes;
        bytes.fill(0x5a);
        callback(nullptr, bytes.data() + 1, 8205);
        for (size_t i = 0; i < 4102; ++i) {
            int16_t actual;
            memcpy(&actual, bytes.data() + 1 + i * 2, 2);
            assert(actual == wave[i % wave.size()] / 2);
        }
        assert(bytes.front() == 0x5a && bytes.back() == 0x5a);
        assert(bytes[8205] == 0 && callbacks == 1 && nonzero == 1);
        mixer = {};
        callbacks = nonzero = 0;
        soundVolume = 128;
    }
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    assert(sfx_init());
    SDL_Delay(50);
    {
        sfxSound sound(smw::filesystem::path("coin.wav"));
        assert(sound.play());
        SDL_Delay(1500);
        assert(!sound.isPlaying());
        sound.playLoop(-1);
        SDL_Delay(50);
        assert(sound.isPlaying());
        sound.stop();
        assert(!sound.isPlaying());
    }
    sfxMusic menu(smw::filesystem::path("menu.ogg")), jingle(smw::filesystem::path("stageclear.ogg"));
    menu.play(false, false);
    SDL_Delay(400);
    {
        AudioLock lock;
        assert(musicSamples > 0 && decodedFrames > 0 && musicActive);
    }
    menu.togglePause();
    SDL_Delay(40);
    unsigned long pausedSamples;
    {
        AudioLock lock;
        pausedSamples = musicSamples;
        assert(musicPaused);
    }
    SDL_Delay(100);
    {
        AudioLock lock;
        assert(musicSamples == pausedSamples);
    }
    menu.togglePause();
    SDL_Delay(100);
    assert(menu.isPlaying());
    for (int i = 0; i < 12; i++) {
        jingle.play(true, false);
        SDL_Delay(20);
        menu.play(false, false);
        SDL_Delay(20);
    }
    jingle.play(true, false);
    for (int i = 0; i < 500 && !finishes; i++) {
        SDL_Delay(20);
        nxdk_audio_update();
    }
    assert(finishes && !jingle.isPlaying());
    unsigned long oldLoops;
    {
        AudioLock lock;
        oldLoops = musicLoops;
    }
    jingle.play(false, false);
    for (int i = 0; i < 500; i++) {
        SDL_Delay(20);
        AudioLock lock;
        if (musicLoops > oldLoops)
            break;
    }
    {
        AudioLock lock;
        assert(musicLoops > oldLoops);
    }
    jingle.stop();
    assert(!jingle.isPlaying());
    checkLoopContinuity(jingle);
    const auto hud = nxdk_audio_status();
    assert(hud.callbacks > 0);
    {
        AudioLock lock;
        assert(hud.callbacks <= callbacks && hud.underruns <= underruns && hud.loops <= musicLoops);
    }
    sfx_close();
    SDL_Quit();
    printf("PASS: WAV playback/stop/loop, Ogg resampling, pause/resume, track replacement, EOF callback, "
           "looping, shutdown\n");
}

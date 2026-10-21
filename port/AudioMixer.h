#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// No SDL, allocation, file I/O or locks in mix(); its caller owns synchronization.
namespace nxdk_audio
{
using Samples = std::vector<int16_t>;
struct Voice {
    const Samples *pcm = nullptr;
    size_t offset = 0;
    int loops = 0;
};
class Mixer
{
  public:
    std::array<Voice, 16> voices{};
    unsigned long starts = 0, completed = 0;
    int play(const Samples *pcm, int loops)
    {
        if (!pcm || pcm->empty())
            return -1;
        for (size_t i = 0; i < voices.size(); ++i)
            if (!voices[i].pcm) {
                voices[i] = {pcm, 0, loops};
                ++starts;
                return (int)i;
            }
        return -1;
    }
    void stop(const Samples *pcm = nullptr)
    {
        for (auto &v : voices)
            if (!pcm || v.pcm == pcm)
                v = {};
    }
    bool playing(const Samples *pcm) const
    {
        for (const auto &v : voices)
            if (v.pcm == pcm && pcm)
                return true;
        return false;
    }
    void mix(int16_t *output, size_t samples, int volume)
    {
        for (size_t base = 0; base < samples; base += 512) {
            const size_t count = std::min(size_t(512), samples - base);
            int32_t sums[512];
            for (size_t i = 0; i < count; ++i)
                sums[i] = output[base + i];
            for (auto &v : voices) {
                size_t at = 0;
                while (v.pcm && at < count) {
                    const size_t n = std::min(count - at, v.pcm->size() - v.offset);
                    for (size_t i = 0; i < n; ++i)
                        sums[at + i] += (*v.pcm)[v.offset + i] * volume / 128;
                    at += n;
                    v.offset += n;
                    if (v.offset == v.pcm->size()) {
                        if (v.loops) {
                            if (v.loops > 0)
                                --v.loops;
                            v.offset = 0;
                        } else {
                            v = {};
                            ++completed;
                        }
                    }
                }
            }
            for (size_t i = 0; i < count; ++i)
                output[base + i] = (int16_t)std::clamp(sums[i], -32768, 32767);
        }
    }
};
} // namespace nxdk_audio

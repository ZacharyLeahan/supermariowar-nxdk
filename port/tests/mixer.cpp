#include "AudioMixer.h"
#include <array>
#include <cassert>
using namespace nxdk_audio;
int main()
{
    Mixer m;
    Samples a{30000, -30000, 1000, -1000};
    std::array<int16_t, 8> out{};
    assert(m.play(&a, 0) == 0);
    assert(m.play(&a, 0) == 1);
    m.mix(out.data(), 4, 128);
    assert(out[0] == 32767 && out[1] == -32768 && out[2] == 2000 && out[3] == -2000);
    assert(!m.playing(&a) && m.completed == 2);
    Samples b{100, -100};
    out.fill(0);
    m.play(&b, 2);
    m.mix(out.data(), 8, 64);
    for (int i = 0; i < 6; i++)
        assert(out[i] == (i % 2 ? -50 : 50));
    assert(out[6] == 0 && !m.playing(&b));
    m.play(&b, -1);
    out.fill(0);
    m.mix(out.data(), 8, 128);
    assert(m.playing(&b));
    m.stop(&b);
    assert(!m.playing(&b));
    for (int i = 0; i < 16; i++)
        assert(m.play(&b, -1) >= 0);
    assert(m.play(&b, 0) == -1);
    m.stop();
    Samples longSound(1200, 2000);
    std::array<int16_t, 1400> large{};
    m.play(&longSound, 0);
    m.mix(large.data(), 1400, 128);
    assert(large[1199] == 2000 && large[1200] == 0 && !m.playing(&longSound));
}

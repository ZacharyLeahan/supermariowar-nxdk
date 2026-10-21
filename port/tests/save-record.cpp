#include "SaveRecord.h"
#include <cassert>
#include <cstdio>
using namespace nxdk_save;
int main()
{
    const std::array<uint8_t, 9> check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(detail::checksum(check) == 0xcbf43926u);
    std::vector<uint8_t> payload(257);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = uint8_t(i);
    const auto good = encode(42, payload);
    Record saved;
    assert(decode(good, saved) && saved.generation == 42 && saved.payload == payload);
    // Every truncated prefix and every single-bit corruption must be rejected,
    // preserving the caller's existing settings record.
    for (size_t n = 0; n < good.size(); ++n) {
        Record keep{7, {9}};
        assert(!decode(std::span(good.data(), n), keep));
        assert(keep.generation == 7 && keep.payload == std::vector<uint8_t>{9});
    }
    for (size_t i = 0; i < good.size(); ++i)
        for (unsigned bit = 0; bit < 8; ++bit) {
            auto bad = good;
            bad[i] ^= uint8_t(1u << bit);
            Record keep{7, {9}};
            assert(!decode(bad, keep));
            assert(keep.generation == 7 && keep.payload == std::vector<uint8_t>{9});
        }
    auto bad = good;
    bad.push_back(0);
    assert(!decode(bad, saved));
    bad = good;
    detail::write32(bad, 8, 2);
    detail::write32(bad, 20, detail::checksum(bad));
    assert(!decode(bad, saved)); // unknown schema even with valid checksum
    bad = good;
    detail::write32(bad, 16, 0xffffffffu);
    detail::write32(bad, 20, detail::checksum(bad));
    assert(!decode(bad, saved));
    std::vector<uint8_t> large(maxPayload, 0x5a);
    assert(decode(encode(0, large), saved) && saved.payload == large);
    large.push_back(0);
    assert(encode(1, large).empty());
    assert(decode(encode(0, {}), saved) && saved.payload.empty());
    Record a{0xfffffffeu, {1}}, b{1, {2}};
    assert(newest(nullptr, nullptr) == -1 && newest(&a, nullptr) == 0 && newest(nullptr, &b) == 1);
    assert(newest(&a, &b) == 1 && newest(&b, &a) == 0); // generation wrap
    a.generation = b.generation;
    assert(newest(&a, &b) == 0); // deterministic tie
    a = {40, {1}};
    b = {41, {2}};
    auto torn = encode(b.generation, b.payload);
    torn.resize(torn.size() - 1);
    Record candidate;
    const bool valid = decode(torn, candidate);
    assert(!valid && newest(&a, valid ? &candidate : nullptr) == 0);
    puts("PASS: bounded save records, corruption/truncation recovery, schema and generation "
         "rollover");
}

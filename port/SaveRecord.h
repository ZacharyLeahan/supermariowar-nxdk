#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// Bounded, endian-defined records for an alternating two-file settings store.
// CRC detects accidental corruption; it is not authentication. No file I/O here.
namespace nxdk_save
{
constexpr size_t maxPayload = 16 * 1024;
constexpr size_t headerSize = 24;
constexpr std::array<uint8_t, 8> magic{'S', 'M', 'W', 'N', 'X', 'C', 'F', 'G'};
struct Record {
    uint32_t generation = 0;
    std::vector<uint8_t> payload;
};
namespace detail
{
inline uint32_t read32(std::span<const uint8_t> bytes, size_t at)
{
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i)
        n |= uint32_t(bytes[at + i]) << (8 * i);
    return n;
}
inline void write32(std::span<uint8_t> bytes, size_t at, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = uint8_t(n >> (8 * i));
}
inline uint32_t checksum(std::span<const uint8_t> bytes)
{
    uint32_t crc = ~uint32_t(0);
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i >= 20 && i < 24)
            continue; // checksum field itself
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
} // namespace detail
inline std::vector<uint8_t> encode(uint32_t generation, std::span<const uint8_t> payload)
{
    if (payload.size() > maxPayload)
        return {};
    std::vector<uint8_t> bytes(headerSize + payload.size(), 0);
    std::copy(magic.begin(), magic.end(), bytes.begin());
    detail::write32(bytes, 8, 1); // record schema, independent of game version
    detail::write32(bytes, 12, generation);
    detail::write32(bytes, 16, uint32_t(payload.size()));
    std::copy(payload.begin(), payload.end(), bytes.begin() + headerSize);
    detail::write32(bytes, 20, detail::checksum(bytes));
    return bytes;
}
inline bool decode(std::span<const uint8_t> bytes, Record &out)
{
    if (bytes.size() < headerSize || bytes.size() > headerSize + maxPayload ||
        !std::equal(magic.begin(), magic.end(), bytes.begin()) || detail::read32(bytes, 8) != 1 ||
        detail::read32(bytes, 16) != bytes.size() - headerSize ||
        detail::read32(bytes, 20) != detail::checksum(bytes))
        return false;
    Record parsed;
    parsed.generation = detail::read32(bytes, 12);
    parsed.payload.assign(bytes.begin() + headerSize, bytes.end());
    out = std::move(parsed); // do not partially apply an invalid record
    return true;
}
inline bool newer(uint32_t a, uint32_t b)
{
    const uint32_t distance = a - b;
    return distance != 0 && distance < 0x80000000u;
}
// Null pointers represent missing/corrupt slots. Keep the newest valid slot;
// the next save must overwrite the other slot, preserving the current copy.
inline int newest(const Record *slot0, const Record *slot1)
{
    if (!slot0)
        return slot1 ? 1 : -1;
    if (!slot1)
        return 0;
    return newer(slot1->generation, slot0->generation) ? 1 : 0;
}
} // namespace nxdk_save

#pragma once
#include "SaveRecord.h"

namespace nxdk_save
{
// IO::read(slot, bytes) must bound file size before allocating. IO::write(slot,
// bytes) returns true only after its platform write/flush/close checks succeed.
// IO errors never justify overwriting the newest valid slot.
template <class IO> bool load(IO &io, Record &out)
{
    Record records[2];
    bool valid[2]{};
    for (unsigned slot = 0; slot < 2; ++slot) {
        std::vector<uint8_t> bytes;
        valid[slot] = io.read(slot, bytes) && decode(bytes, records[slot]);
    }
    const int selected = newest(valid[0] ? &records[0] : nullptr, valid[1] ? &records[1] : nullptr);
    if (selected < 0)
        return false;
    out = std::move(records[selected]);
    return true;
}
template <class IO>
bool save(IO &io, std::span<const uint8_t> payload, uint32_t *generation = nullptr)
{
    if (payload.size() > maxPayload)
        return false;
    Record records[2];
    bool valid[2]{};
    for (unsigned slot = 0; slot < 2; ++slot) {
        std::vector<uint8_t> bytes;
        valid[slot] = io.read(slot, bytes) && decode(bytes, records[slot]);
    }
    const int current = newest(valid[0] ? &records[0] : nullptr, valid[1] ? &records[1] : nullptr);
    const unsigned target = current == 0 ? 1 : 0;
    const uint32_t next = current < 0 ? 1 : records[current].generation + 1;
    const auto encoded = encode(next, payload);
    if (!io.write(target, encoded))
        return false;
    // Readback verifies the complete record, not just the write call's length.
    std::vector<uint8_t> bytes;
    Record verified;
    if (!io.read(target, bytes) || !decode(bytes, verified) || verified.generation != next ||
        verified.payload.size() != payload.size() ||
        !std::equal(payload.begin(), payload.end(), verified.payload.begin()))
        return false;
    if (generation)
        *generation = next;
    return true;
}
} // namespace nxdk_save

#include "SaveStore.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace nxdk_save;
struct Disk {
    std::vector<uint8_t> slots[2];
    size_t failAfter = std::numeric_limits<size_t>::max();
    int lastWritten = -1;
    bool read(unsigned slot, std::vector<uint8_t> &out)
    {
        if (slots[slot].empty() || slots[slot].size() > headerSize + maxPayload)
            return false;
        out = slots[slot];
        return true;
    }
    bool write(unsigned slot, std::span<const uint8_t> bytes)
    {
        lastWritten = int(slot);
        slots[slot].assign(bytes.begin(), bytes.begin() + std::min(bytes.size(), failAfter));
        return failAfter >= bytes.size();
    }
};
int main()
{
    Disk disk;
    Record out{99, {99}};
    assert(!load(disk, out) && out.generation == 99);
    const std::vector<uint8_t> old{1, 2, 3}, next{4, 5, 6, 7};
    uint32_t generation = 0;
    assert(save(disk, old, &generation) && generation == 1 && disk.lastWritten == 0);
    const auto first = disk.slots[0];
    assert(save(disk, next, &generation) && generation == 2 && disk.lastWritten == 1);
    assert(disk.slots[0] == first && load(disk, out) && out.payload == next);
    assert(save(disk, old, &generation) && generation == 3 && disk.lastWritten == 0);
    assert(load(disk, out) && out.generation == 3 && out.payload == old);
    // Simulate interruption at every byte boundary of an alternate-slot write.
    const auto complete = encode(4, next);
    for (size_t n = 0; n < complete.size(); ++n) {
        auto interrupted = disk;
        interrupted.failAfter = n;
        const auto preserved = interrupted.slots[0];
        assert(!save(interrupted, next));
        assert(interrupted.lastWritten == 1 && interrupted.slots[0] == preserved);
        assert(load(interrupted, out) && out.generation == 3 && out.payload == old);
    }
    disk.slots[0][0] ^= 1; // newer copy corrupt; older valid slot survives
    assert(load(disk, out) && out.generation == 2 && out.payload == next);
    disk.slots[0] = encode(0xffffffffu, old);
    disk.slots[1].clear();
    assert(save(disk, next, &generation) && generation == 0 && disk.lastWritten == 1);
    assert(load(disk, out) && out.generation == 0 && out.payload == next);
    std::vector<uint8_t> tooLarge(maxPayload + 1);
    const auto before0 = disk.slots[0], before1 = disk.slots[1];
    assert(!save(disk, tooLarge) && disk.slots[0] == before0 && disk.slots[1] == before1);
    puts("PASS: alternating save slots, readback, interrupted writes and fallback");
}

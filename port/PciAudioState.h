#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xboxkrnl/xboxkrnl.h>

/* Read-only diagnostic. Check candidate raw HAL slot encodings rather than guessing a
   vendor/device ID or slot-number ABI. Select multimedia functions and any
   function whose memory BAR matches the AC97 MMIO address used by this SDK. */
static void nxdk_log_pci_audio(const char *phase)
{
    unsigned found = 0;
    const unsigned slots[] = {5, 6, 0x28, 0x30};
    for (unsigned candidate = 0; candidate < sizeof(slots)/sizeof(slots[0]); ++candidate) {
        const unsigned slot = slots[candidate];
        printf("[audio-pci] phase=%s reading_raw_slot=%u\n", phase, slot);
        uint32_t config[16];
        memset(config, 0xff, sizeof(config));
        // Four-byte accesses match the existing nxdk kernel-API callers.
        for (unsigned word = 0; word < 16; ++word)
            HalReadWritePCISpace(0, slot, word * 4, &config[word], 4, FALSE);
        if ((config[0] & 0xffff) == 0xffff || (config[0] & 0xffff) == 0) continue;
        int ac97_bar = 0;
        for (unsigned bar = 4; bar < 10; ++bar)
            if (!(config[bar] & 1) && (config[bar] & 0xfffffff0u) == 0xfec00000u) ac97_bar = 1;
        if (!ac97_bar && (config[2] >> 24) != 4) continue;
        ++found;
        printf("[audio-pci] phase=%s raw_slot=%u id=%08lx command_status=%08lx "
               "class_revision=%08lx ac97_bar_match=%d bar0=%08lx bar1=%08lx "
               "bar2=%08lx bar3=%08lx bar4=%08lx bar5=%08lx\n", phase, slot,
               (unsigned long)config[0], (unsigned long)config[1],
               (unsigned long)config[2], ac97_bar, (unsigned long)config[4],
               (unsigned long)config[5], (unsigned long)config[6], (unsigned long)config[7],
               (unsigned long)config[8], (unsigned long)config[9]);
    }
    printf("[audio-pci] phase=%s functions_logged=%u configuration_writes=0\n", phase, found);
}

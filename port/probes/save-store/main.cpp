#include "../../SaveFileSlots.h"
#include "../../SaveStore.h"
#include <cstdio>
#include <hal/video.h>
#include <hal/xbox.h>

unsigned failures;
void check(bool condition, const char *label)
{
    printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        ++failures;
}
int main()
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);
    freopen("D:\\save-store.log", "w", stdout);
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("Xbox two-slot save probe; writes only probe-owned files in this folder\n");
    nxdk_save::FileSlots disk("D:\\probe-save");
    nxdk_save::Record record;
    const std::vector<uint8_t> one{1, 2, 3}, two{4, 5, 6, 7}, final{8, 9, 10, 11, 12};
    const bool previous = nxdk_save::load(disk, record);
    printf("previous_record=%s generation=%lu matches_final=%d\n",
           previous ? "valid" : "absent-or-invalid", (unsigned long)record.generation,
           previous && record.payload == final);
    // This directory is dedicated to this probe; never delete a game save.
    DeleteFileA(disk.path(0));
    DeleteFileA(disk.path(1));
    check(!nxdk_save::load(disk, record), "missing slots recover to defaults");
    uint32_t generation = 0;
    check(nxdk_save::save(disk, one, &generation) && generation == 1,
          "first durable slot + readback");
    check(nxdk_save::save(disk, two, &generation) && generation == 2,
          "alternate durable slot + readback");
    check(nxdk_save::load(disk, record) && record.generation == 2 && record.payload == two,
          "newest slot selected");
    // Create an intentionally truncated newer copy, then close/flush it.
    HANDLE file = CreateFileA(disk.path(1), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    DWORD written = 0;
    const uint8_t torn[7] = {'S', 'M', 'W', 'N', 'X', 'C', 'F'};
    bool truncated = file != INVALID_HANDLE_VALUE;
    if (truncated) {
        truncated =
            WriteFile(file, torn, sizeof(torn), &written, nullptr) && written == sizeof(torn);
        IO_STATUS_BLOCK io{};
        const NTSTATUS status = NtFlushBuffersFile(file, &io);
        truncated = truncated && NT_SUCCESS(status) && status != STATUS_PENDING;
        truncated = CloseHandle(file) && truncated;
    }
    check(truncated, "created flushed truncated newer slot");
    check(nxdk_save::load(disk, record) && record.generation == 1 && record.payload == one,
          "older valid slot survives torn write");
    check(nxdk_save::save(disk, two, &generation) && generation == 2,
          "replace damaged alternate slot");
    check(nxdk_save::save(disk, final, &generation) && generation == 3,
          "commit final reboot-check record");
    check(nxdk_save::load(disk, record) && record.generation == 3 && record.payload == final,
          "final record readback");
    nxdk_save::FileSlots bad("D:\\missing-probe-directory\\save");
    check(!nxdk_save::save(bad, one), "unwritable path reports failure");
    printf("write_error=%lu flush_status=%ld failures=%u\n", (unsigned long)disk.lastError,
           (long)disk.lastFlushStatus, failures);
    printf("result=%s\n", failures ? "failed" : "passed");
    printf(
        "Run again after reboot: previous_record must be valid, generation=3, matches_final=1\n");
    fflush(stdout);
    Sleep(1000);
    XReboot();
    return failures ? 1 : 0;
}

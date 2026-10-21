#pragma once
#include "SaveRecord.h"
#include <string>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

namespace nxdk_save
{
// Xbox-only two-slot file adapter. A separate probe verifies this on FATX before
// game settings use it. Paths are explicit so tests cannot touch game files.
class FileSlots
{
    std::array<std::string, 2> paths;
    struct File {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~File()
        {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
        bool close()
        {
            const BOOL ok = CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            return ok != FALSE;
        }
    };

  public:
    DWORD lastError = 0;
    NTSTATUS lastFlushStatus = STATUS_SUCCESS;
    explicit FileSlots(const std::string &prefix) : paths{prefix + "0.bin", prefix + "1.bin"} {}
    const char *path(unsigned slot) const { return slot < 2 ? paths[slot].c_str() : nullptr; }
    bool read(unsigned slot, std::vector<uint8_t> &out)
    {
        if (slot >= 2) {
            lastError = ERROR_INVALID_PARAMETER;
            return false;
        }
        File file;
        file.handle = CreateFileA(path(slot), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file.handle == INVALID_HANDLE_VALUE) {
            lastError = GetLastError();
            return false;
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.handle, &size)) {
            lastError = GetLastError();
            return false;
        }
        if (size.QuadPart < headerSize || size.QuadPart > headerSize + maxPayload) {
            lastError = ERROR_INVALID_DATA;
            return false;
        }
        std::vector<uint8_t> bytes(size_t(size.QuadPart));
        DWORD got = 0;
        if (!ReadFile(file.handle, bytes.data(), DWORD(bytes.size()), &got, nullptr)) {
            lastError = GetLastError();
            return false;
        }
        if (got != bytes.size()) {
            lastError = ERROR_HANDLE_EOF;
            return false;
        }
        if (!file.close()) {
            lastError = GetLastError();
            return false;
        }
        out = std::move(bytes);
        lastError = 0;
        return true;
    }
    bool write(unsigned slot, std::span<const uint8_t> bytes)
    {
        if (slot >= 2 || bytes.size() < headerSize || bytes.size() > headerSize + maxPayload) {
            lastError = ERROR_INVALID_PARAMETER;
            return false;
        }
        File file;
        file.handle = CreateFileA(path(slot), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file.handle == INVALID_HANDLE_VALUE) {
            lastError = GetLastError();
            return false;
        }
        DWORD written = 0;
        if (!WriteFile(file.handle, bytes.data(), DWORD(bytes.size()), &written, nullptr)) {
            lastError = GetLastError();
            return false;
        }
        if (written != bytes.size()) {
            lastError = ERROR_WRITE_FAULT;
            return false;
        }
        IO_STATUS_BLOCK status{};
        lastFlushStatus = NtFlushBuffersFile(file.handle, &status);
        if (!NT_SUCCESS(lastFlushStatus) || lastFlushStatus == STATUS_PENDING) {
            lastError = RtlNtStatusToDosError(lastFlushStatus);
            return false;
        }
        if (!file.close()) {
            lastError = GetLastError();
            return false;
        }
        lastError = 0;
        return true;
    }
};
} // namespace nxdk_save

#pragma once
#include "SaveRecord.h"
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace nxdk_save {
inline constexpr size_t PreferenceAssetCount = 11;
inline constexpr size_t MaxAssetNameBytes = 255;
inline bool assetNameEqual(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        if (fold(a[i]) != fold(b[i])) return false;
    }
    return true;
}
inline bool validAssetName(std::string_view name)
{
    if (name.empty() || name.size() > MaxAssetNameBytes || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
    return true;
}
template <class Config> bool assetPreferencesValid(const Config &c)
{
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.assetNames)>> == PreferenceAssetCount);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.assetChoices)>> == PreferenceAssetCount);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.assetFallback)>> == PreferenceAssetCount);
    for (size_t i = 0; i < PreferenceAssetCount; ++i) {
        if (!validAssetName(c.assetNames[i])) return false;
        bool found = false;
        for (const auto &choice : c.assetChoices[i]) found |= assetNameEqual(choice, c.assetNames[i]);
        if (!found) return false;
    }
    return true;
}
template <class Config> bool appendAssetPreferences(std::vector<uint8_t> &bytes, const Config &c)
{
    if (!assetPreferencesValid(c)) return false;
    const auto append32 = [&](uint32_t value) {
        const size_t at = bytes.size(); bytes.resize(at + 4); detail::write32(bytes, at, value);
    };
    append32(PreferenceAssetCount);
    for (const auto &name : c.assetNames) {
        append32(name.size());
        bytes.insert(bytes.end(), name.begin(), name.end());
    }
    return true;
}
template <class Config> bool readAssetPreferences(std::span<const uint8_t> bytes, size_t at, Config &c)
{
    if (at > bytes.size() || bytes.size() - at < 4 || detail::read32(bytes, at) != PreferenceAssetCount)
        return false;
    at += 4;
    for (size_t i = 0; i < PreferenceAssetCount; ++i) {
        if (bytes.size() - at < 4) return false;
        const uint32_t length = detail::read32(bytes, at); at += 4;
        if (!length || length > MaxAssetNameBytes || length > bytes.size() - at) return false;
        const std::string name(reinterpret_cast<const char *>(bytes.data() + at), length); at += length;
        if (!validAssetName(name)) return false;
        bool found = false;
        for (const auto &choice : c.assetChoices[i]) {
            if (assetNameEqual(choice, name)) { c.assetNames[i] = choice; found = true; break; }
        }
        // Removed packs/skins retain the current valid default, without
        // discarding unrelated preferences or creating an invalid list index.
        c.assetFallback[i] = !found;
    }
    return at == bytes.size() && assetPreferencesValid(c);
}
} // namespace nxdk_save

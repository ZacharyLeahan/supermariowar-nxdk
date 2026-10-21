#pragma once
#include "SaveRecord.h"
#include "PreferenceAssets.h"
#include <type_traits>
#include <initializer_list>
#include <tuple>

namespace nxdk_save
{
// Explicit global audio/gameplay toggles (schema 1), then powerup presets
// (schema 2), global menu settings (schema 3) and mode settings (schema 4).
// Mode goals are appended in schema 5; random skins and stable asset names
// follow in schema 6, controller profiles and player choices in schema 7.
// Schema 8 identifies joystick devices by physical port slot, not SDL index.
// Changing this list/order requires a payload schema migration/version bump.
template <class Config, class Visit> void preferenceFields(Config &c, Visit visit)
{
    visit(c.sound, 0, 1);
    visit(c.music, 0, 1);
    visit(c.soundvolume, 0, 128);
    visit(c.musicvolume, 0, 128);
    visit(c.cpudifficulty, 0, 4);
    visit(c.showwinningcrown, 0, 1);
    visit(c.secretsenabled, 0, 1);
    visit(c.startgamecountdown, 0, 1);
    visit(c.deadteamnotice, 0, 1);
    visit(c.startmodedisplay, 0, 1);
    visit(c.playnextmusic, 0, 1);
}
inline constexpr uint32_t PreferenceSchema = 8;
inline constexpr uint32_t PreferenceCountV2 = 11 + 2 + 17 * 26;
inline constexpr uint32_t PreferenceGameplayCount = 0
#define NXDK_PREF(field, low, high, ...) + 1
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
;
inline constexpr uint32_t PreferenceCountV3 = PreferenceCountV2 + PreferenceGameplayCount;
inline constexpr uint32_t PreferenceModeCount = 0
#define NXDK_MODE_PREF(field, low, high, ...) + 1
#include "PreferenceModeFields.inc"
#undef NXDK_MODE_PREF
;
inline constexpr uint32_t PreferenceCountV4 = PreferenceCountV3 + PreferenceModeCount;
inline constexpr uint32_t PreferenceGoalCount = 22;
inline constexpr uint32_t PreferenceCountV5 = PreferenceCountV4 + PreferenceGoalCount;
inline constexpr uint32_t PreferenceCountV6 = PreferenceCountV5 + 4;
inline constexpr uint32_t PreferenceCount = PreferenceCountV6 + 4 * (2 + 2 * 17);
static_assert(12 + PreferenceCount * 4 + 4 + PreferenceAssetCount * (4 + MaxAssetNameBytes) <= maxPayload);
template <class Config, class Visit> void preferenceFieldsV2(Config &c, Visit visit)
{
    preferenceFields(c, visit);
    visit(c.poweruppreset, 0, 16);
    visit(c.overridepowerupsettings, 0, 3);
    // Fixed schema dimensions; an upstream shape change needs a migration.
    static_assert(std::extent_v<decltype(c.allPowerupPresets), 0> == 17);
    static_assert(std::extent_v<decltype(c.allPowerupPresets), 1> == 26);
    for (auto &preset : c.allPowerupPresets)
        for (auto &weight : preset)
            visit(weight, 0, 10);
}
template <class Config, class Visit> void preferenceFieldsV3(Config &c, Visit visit)
{
    preferenceFieldsV2(c, visit);
#define NXDK_PREF(field, low, high, ...) visit(c.field, low, high);
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
}
inline bool preferenceChoice(int value, std::initializer_list<int> choices)
{
    for (int choice : choices) if (value == choice) return true;
    return false;
}
template <class Config> bool gameplayPreferencesValid(const Config &c)
{
#define NXDK_PREF(field, low, high, ...) if (!preferenceChoice(int(c.field), {__VA_ARGS__})) return false;
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
    return true;
}
template <class Config, class Visit> void preferenceFieldsV4(Config &c, Visit visit)
{
    preferenceFieldsV3(c, visit);
    // An upstream array shape change requires a payload migration.
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.stomp.enemyweight)>> == 9);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.egg.eggs)>> == 4);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.egg.yoshis)>> == 4);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.survival.enemyweight)>> == 3);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.chase.phantoquantity)>> == 3);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.gamemodemenusettings.frenzy.powerupweight)>> == 19);
#define NXDK_MODE_PREF(field, low, high, ...) visit(c.gamemodemenusettings.field, low, high);
#include "PreferenceModeFields.inc"
#undef NXDK_MODE_PREF
}
template <class Config> bool modePreferencesValid(const Config &c)
{
#define NXDK_MODE_PREF(field, low, high, ...) if (!preferenceChoice(int(c.gamemodemenusettings.field), {__VA_ARGS__})) return false;
#include "PreferenceModeFields.inc"
#undef NXDK_MODE_PREF
    return true;
}
template <class Config, class Visit> void preferenceFieldsV5(Config &c, Visit visit)
{
    preferenceFieldsV4(c, visit);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.modeGoals)>> == PreferenceGoalCount);
    for (auto &goal : c.modeGoals) visit(goal, -1, 32767);
}
template <class Config> bool goalPreferencesValid(const Config &c)
{
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.modeGoalChoices)>> == PreferenceGoalCount);
    for (size_t i = 0; i < PreferenceGoalCount; ++i) {
        bool found = false;
        for (auto choice : c.modeGoalChoices[i]) found |= choice == c.modeGoals[i];
        if (!found) return false;
    }
    return true;
}
template <class Config, class Visit> void preferenceFieldsV6(Config &c, Visit visit)
{
    preferenceFieldsV5(c, visit);
    static_assert(std::extent_v<decltype(c.randomskin)> == 4);
    for (auto &random : c.randomskin) visit(random, 0, 1);
}
// Schema 7: player type and selected profile, followed by keyboard/joystick
// device IDs and each profile's eight game and eight menu bindings.
template <class Config, class Visit> void preferenceFieldsV7(Config &c, Visit visit)
{
    preferenceFieldsV6(c, visit);
    static_assert(std::extent_v<decltype(c.inputConfiguration), 0> == 4);
    static_assert(std::extent_v<decltype(c.inputConfiguration), 1> == 2);
    static_assert(std::extent_v<decltype(c.playercontrol)> == 4);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(c.selectedProfiles)>> == 4);
    using Input = std::remove_reference_t<decltype(c.inputConfiguration[0][0])>;
    static_assert(std::extent_v<decltype(Input::inputGameControls)> == 2);
    using Group = std::remove_reference_t<decltype(c.inputConfiguration[0][0].inputGameControls[0])>;
    static_assert(std::extent_v<decltype(Group::keys)> == 8);
    for (size_t i = 0; i < 4; ++i) {
        visit(c.playercontrol[i], 0, 2);
        visit(c.selectedProfiles[i], 0, 1);
        for (size_t profile = 0; profile < 2; ++profile) {
            auto &input = c.inputConfiguration[i][profile];
            visit(input.iDevice, profile ? 0 : -1, profile ? 3 : -1);
            for (auto &group : input.inputGameControls)
                for (auto &key : group.keys) visit(key, profile ? 0 : -1, profile ? 29 : 0x400001ff);
        }
    }
}
inline bool validKeyboardBinding(int key)
{
    return key == -1 || (key >= 0 && key <= 0x10ffff && !(key >= 0xd800 && key <= 0xdfff)) ||
        (key >= 0x40000000 && key <= 0x400001ff);
}
template <class Config> bool controllerPreferencesValid(const Config &c)
{
    for (size_t i = 0; i < 4; ++i)
        for (const auto &group : c.inputConfiguration[i][0].inputGameControls)
            for (auto key : group.keys) if (!validKeyboardBinding(int(key))) return false;
    return true;
}
template <class Config>
std::vector<uint8_t> encodePreferences(const Config &config, uint32_t gameVersion)
{
    if (!gameplayPreferencesValid(config) || !modePreferencesValid(config) ||
        !goalPreferencesValid(config) || !assetPreferencesValid(config) || !controllerPreferencesValid(config)) return {};
    constexpr uint32_t count = PreferenceCount;
    std::vector<uint8_t> bytes(12 + count * 4);
    detail::write32(bytes, 0, PreferenceSchema);
    detail::write32(bytes, 4, gameVersion);
    detail::write32(bytes, 8, count);
    size_t at = 12;
    bool valid = true;
    preferenceFieldsV7(config, [&](const auto &value, int low, int high) {
        const int n = int(value);
        if (n < low || n > high)
            valid = false;
        detail::write32(bytes, at, uint32_t(n));
        at += 4;
    });
    if (!valid || !appendAssetPreferences(bytes, config)) return {};
    return bytes;
}
template <class Config>
bool decodePreferences(std::span<const uint8_t> bytes, uint32_t gameVersion, Config &config)
{
    if (bytes.size() < 12 || detail::read32(bytes, 4) != gameVersion)
        return false;
    const uint32_t schema = detail::read32(bytes, 0);
    const uint32_t count = schema == 1 ? 11 : (schema == 2 ? PreferenceCountV2 :
        (schema == 3 ? PreferenceCountV3 : (schema == 4 ? PreferenceCountV4 :
        (schema == 5 ? PreferenceCountV5 : (schema == 6 ? PreferenceCountV6 : PreferenceCount)))));
    if ((schema < 1 || schema > PreferenceSchema) ||
        detail::read32(bytes, 8) != count || bytes.size() < 12 + count * 4 ||
        (schema < 6 && bytes.size() != 12 + count * 4))
        return false;
    // Older schemas leave newer fields at the caller's defaults.
    Config staged = config;
    size_t at = 12;
    bool valid = true;
    auto readField = [&](auto &value, int low, int high) {
        const uint32_t raw = detail::read32(bytes, at);
        // nxdk's pinned libc++ has no std::bit_cast. Decode two's-complement
        // integers arithmetically, without an implementation-defined cast.
        const int64_t n = raw <= 0x7fffffffu ? int64_t(raw) : int64_t(raw) - 0x100000000LL;
        at += 4;
        if (n < low || n > high)
            valid = false;
        else
            value = static_cast<std::remove_reference_t<decltype(value)>>(n);
    };
    if (schema == 1) preferenceFields(staged, readField);
    else if (schema == 2) preferenceFieldsV2(staged, readField);
    else if (schema == 3) preferenceFieldsV3(staged, readField);
    else if (schema == 4) preferenceFieldsV4(staged, readField);
    else if (schema == 5) preferenceFieldsV5(staged, readField);
    else if (schema == 6) preferenceFieldsV6(staged, readField);
    else preferenceFieldsV7(staged, readField);
    if (!valid || (schema >= 3 && !gameplayPreferencesValid(staged)) ||
        (schema >= 4 && !modePreferencesValid(staged)) ||
        (schema >= 5 && !goalPreferencesValid(staged)) ||
        (schema >= 7 && !controllerPreferencesValid(staged)))
        return false;
    if (schema >= 6 && !readAssetPreferences(bytes, at, staged)) return false;
    if (schema == 7) {
        static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(staged.legacyDeviceSlots)>> == 4);
        // Import the old dense SDL-device indices through the caller's current
        // enumeration. Missing indices retain their numeric slot; no historical
        // physical identity can be reconstructed for an absent old device.
        for (size_t i = 0; i < 4; ++i) {
            auto &device = staged.inputConfiguration[i][1].iDevice;
            const auto slot = staged.legacyDeviceSlots[device];
            if (slot < 0 || slot > 3) return false;
            device = slot;
        }
    }
    config = std::move(staged);
    return true;
}
// Validate payload semantics before slot selection, not only after choosing
// the newest CRC-valid record. An unsupported newer slot can then fall back.
template <class IO, class Config> struct PreferenceSlots {
    IO &files;
    const Config &defaults;
    uint32_t gameVersion;
    bool read(unsigned slot, std::vector<uint8_t> &bytes)
    {
        if (!files.read(slot, bytes))
            return false;
        Record record;
        if (!decode(bytes, record))
            return false;
        auto staged = defaults;
        return decodePreferences(record.payload, gameVersion, staged);
    }
    bool write(unsigned slot, std::span<const uint8_t> bytes) { return files.write(slot, bytes); }
};
} // namespace nxdk_save

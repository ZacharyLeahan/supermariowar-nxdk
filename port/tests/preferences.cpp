#include "PreferencesCodec.h"
#include "SaveStore.h"
#include "GameModeSettings.h"
#include <cassert>
#include <cstdio>
struct TestModes : GameModeSettings {
    bool operator==(const TestModes &other) const {
        return classic == other.classic &&
            frag == other.frag &&
            time == other.time &&
            jail == other.jail &&
            coins == other.coins &&
            stomp == other.stomp &&
            egg == other.egg &&
            flag == other.flag &&
            chicken == other.chicken &&
            tag == other.tag &&
            star == other.star &&
            domination == other.domination &&
            kingofthehill == other.kingofthehill &&
            race == other.race &&
            frenzy == other.frenzy &&
            survival == other.survival &&
            greed == other.greed &&
            health == other.health &&
            collection == other.collection &&
            chase == other.chase &&
            shyguytag == other.shyguytag &&
            boss == other.boss;
    }
};
struct TestInputGroup { int keys[8]{}; bool operator==(const TestInputGroup &) const = default; };
struct TestInput { short iDevice = -1; TestInputGroup inputGameControls[2]; bool operator==(const TestInput &) const = default; };
struct Config {
    TestInput inputConfiguration[4][2];
    short playercontrol[4]{1,2,0,0};
    std::array<short,4> selectedProfiles{1,1,1,1};
    std::array<short,4> legacyDeviceSlots{0,1,2,3};
    TestModes gamemodemenusettings;
    std::array<short, 22> modeGoals{};
    bool randomskin[4]{};
    std::array<std::string, 11> assetNames{};
    std::array<std::vector<std::string>, 11> assetChoices{};
    std::array<bool, 11> assetFallback{};
    std::array<std::array<short, 21>, 22> modeGoalChoices{};
    Config() {
        for (size_t i = 0; i < 4; ++i) inputConfiguration[i][1].iDevice = i;
        for (size_t i = 0; i < 11; ++i) { assetNames[i] = "Classic"; assetChoices[i] = {"Classic", "Choice00" + std::string(1, char('A' + i))}; }
        for (size_t i = 0; i < 22; ++i) {
            for (size_t j = 0; j < 20; ++j) modeGoalChoices[i][j] = (j + 1) * 5;
            modeGoalChoices[i][20] = -1;
            modeGoals[i] = 10;
        }
    }
    bool sound = true, music = true;
    short soundvolume = 128, musicvolume = 128, cpudifficulty = 2;
    bool showwinningcrown = true, secretsenabled = false, startgamecountdown = true;
    bool deadteamnotice = true, startmodedisplay = true, playnextmusic = false;
    short poweruppreset = 0, overridepowerupsettings = 0;
    short allPowerupPresets[17][26]{};
#define NXDK_PREF(field, low, high, ...) short field = low;
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
    void *runtimeInput = nullptr; // must never enter a save payload or change on decode
    bool operator==(const Config &) const = default;
};
struct Disk {
    std::vector<uint8_t> slots[2];
    bool read(unsigned slot, std::vector<uint8_t> &bytes)
    {
        bytes = slots[slot];
        return !bytes.empty();
    }
    bool write(unsigned slot, std::span<const uint8_t> bytes)
    {
        slots[slot].assign(bytes.begin(), bytes.end());
        return true;
    }
};
int main()
{
    constexpr uint32_t version = 0x02000001;
    // A field-list extension must explicitly revise the on-disk contract/tests.
    static_assert(nxdk_save::PreferenceSchema == 8);
    static_assert(nxdk_save::PreferenceGameplayCount == 44);
    static_assert(nxdk_save::PreferenceCountV3 == 499);
    static_assert(nxdk_save::PreferenceModeCount == 105);
    static_assert(nxdk_save::PreferenceCountV4 == 604);
    static_assert(nxdk_save::PreferenceCountV5 == 626);
    static_assert(nxdk_save::PreferenceCountV6 == 630);
    static_assert(nxdk_save::PreferenceCount == 774);
    Config original;
    int marker;
    original.runtimeInput = &marker;
    assert(original.gamemodemenusettings.health.startlife == 6);
    assert(original.gamemodemenusettings.health.maxlife == 10);
    Config changed = original;
    changed.sound = false;
    changed.music = false;
    changed.soundvolume = 16;
    changed.showwinningcrown = false;
    changed.secretsenabled = true;
    changed.startgamecountdown = false;
    changed.deadteamnotice = false;
    changed.startmodedisplay = false;
    changed.playnextmusic = true;
    changed.musicvolume = 32;
    changed.cpudifficulty = 4;
    changed.poweruppreset = 16;
    changed.overridepowerupsettings = 3;
    for (unsigned i = 0; i < 17; ++i)
        for (unsigned j = 0; j < 26; ++j)
            changed.allPowerupPresets[i][j] = (i + j) % 11;
#define NXDK_PREF(field, low, high, ...) changed.field = high;
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
#define NXDK_MODE_PREF(field, low, high, ...) changed.gamemodemenusettings.field = static_cast<std::remove_cvref_t<decltype(changed.gamemodemenusettings.field)>>(high);
#include "PreferenceModeFields.inc"
#undef NXDK_MODE_PREF
    // Distinct array values expose dropped/duplicated elements during decode.
    for (size_t i = 0; i < changed.gamemodemenusettings.stomp.enemyweight.size(); ++i)
        changed.gamemodemenusettings.stomp.enemyweight[i] = i % 11;
    for (size_t i = 0; i < changed.gamemodemenusettings.frenzy.powerupweight.size(); ++i)
        changed.gamemodemenusettings.frenzy.powerupweight[i] = i % 11;
    changed.gamemodemenusettings.egg.eggs = {0, 1, 2, 3};
    changed.gamemodemenusettings.egg.yoshis = {4, 3, 2, 1};
    changed.gamemodemenusettings.survival.enemyweight = {1, 2, 3};
    changed.gamemodemenusettings.chase.phantoquantity = {0, 1, 2};
    for (size_t i = 0; i < 22; ++i) changed.modeGoals[i] = (i % 20 + 1) * 5;
    changed.modeGoals[21] = -1;
    for (size_t i = 0; i < 11; ++i) changed.assetNames[i] = changed.assetChoices[i][1];
    for (size_t i = 0; i < 4; ++i) changed.randomskin[i] = i % 2;
    const auto good = nxdk_save::encodePreferences(changed, version);
    assert(good.size() == 12 + nxdk_save::PreferenceCount * 4 + 4 + 11 * (4 + 9));
    assert(nxdk_save::detail::read32(good, 0) == 8);
    assert(nxdk_save::detail::read32(good, 8) == nxdk_save::PreferenceCount);
    assert(nxdk_save::detail::read32(good, 56) == 16);
    assert(nxdk_save::detail::read32(good, 60) == 3);
    for (unsigned i = 0; i < 17; ++i)
        for (unsigned j = 0; j < 26; ++j)
            assert(nxdk_save::detail::read32(good, 64 + (i * 26 + j) * 4) == (i + j) % 11);
    const std::array<uint32_t, 11> expected{0, 0, 16, 32, 4, 0, 1, 0, 0, 0, 1};
    for (size_t i = 0; i < expected.size(); ++i)
        assert(nxdk_save::detail::read32(good, 12 + 4 * i) == expected[i]);
    assert(nxdk_save::detail::read32(good, 1832) == 5); // awardstyle
    assert(nxdk_save::detail::read32(good, 1832 + 25 * 4) == 20); // respawn
    assert(nxdk_save::detail::read32(good, 1832 + 43 * 4) == 620); // warplocktime
    assert(nxdk_save::detail::read32(good, 2428) == 5);
    assert(nxdk_save::detail::read32(good, 2428 + 21 * 4) == 0xffffffffu);
    Config target = original;
    assert(nxdk_save::decodePreferences(good, version, target) && target == changed);
    // Legacy Health values must not discard unrelated preferences or select an
    // older save. Freeze independent wire offsets for the two migrated fields.
    for (unsigned maximum : {1u, 2u, 6u, 10u}) {
        auto oldHealth = good;
        nxdk_save::detail::write32(oldHealth, 2272, maximum);
        nxdk_save::detail::write32(oldHealth, 2280, 6);
        target = original;
        assert(nxdk_save::decodePreferences(oldHealth, version, target));
        Config expectedHealth = changed;
        expectedHealth.gamemodemenusettings.health.startlife = 6;
        expectedHealth.gamemodemenusettings.health.maxlife = maximum == 1 ? 10 : std::max(6u, maximum);
        assert(target == expectedHealth);
        auto migrated = nxdk_save::encodePreferences(target, version);
        Config reloaded = original;
        assert(nxdk_save::decodePreferences(migrated, version, reloaded) && reloaded == target);
    }
    target = changed;
    target.gamemodemenusettings.health.startlife = 6;
    target.gamemodemenusettings.health.maxlife = 2;
    assert(nxdk_save::encodePreferences(target, version).empty());
    for (size_t n = 0; n < good.size(); ++n) {
        target = original;
        assert(!nxdk_save::decodePreferences(std::span(good.data(), n), version, target) &&
               target == original);
    }
    for (size_t field = 0; field < nxdk_save::PreferenceCount; ++field) {
        auto bad = good;
        nxdk_save::detail::write32(bad, 12 + 4 * field, 0x7fffffffu);
        target = original;
        assert(!nxdk_save::decodePreferences(bad, version, target) && target == original);
    }
    auto bad = good;
    nxdk_save::detail::write32(bad, 0, 99);
    assert(!nxdk_save::decodePreferences(bad, version, target));
    assert(!nxdk_save::decodePreferences(good, version + 1, target));
    target = changed;
    target.musicvolume = 129;
    assert(nxdk_save::encodePreferences(target, version).empty());
    // Legacy payload retains current defaults for fields it never contained.
    auto legacy = good;
    legacy.resize(56);
    nxdk_save::detail::write32(legacy, 0, 1);
    nxdk_save::detail::write32(legacy, 8, 11);
    target = original;
    assert(nxdk_save::decodePreferences(legacy, version, target));
    Config expectedLegacy = changed;
    expectedLegacy.poweruppreset = original.poweruppreset;
    expectedLegacy.overridepowerupsettings = original.overridepowerupsettings;
    for (unsigned i = 0; i < 17; ++i)
        for (unsigned j = 0; j < 26; ++j)
            expectedLegacy.allPowerupPresets[i][j] = original.allPowerupPresets[i][j];
#define NXDK_PREF(field, low, high, ...) expectedLegacy.field = original.field;
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
    expectedLegacy.gamemodemenusettings = original.gamemodemenusettings;
    expectedLegacy.modeGoals = original.modeGoals;
    expectedLegacy.assetNames = original.assetNames;
    for (size_t i = 0; i < 4; ++i) expectedLegacy.randomskin[i] = original.randomskin[i];
    assert(target == expectedLegacy);
    auto v2 = good;
    v2.resize(1832);
    nxdk_save::detail::write32(v2, 0, 2);
    nxdk_save::detail::write32(v2, 8, 455);
    target = original;
    assert(nxdk_save::decodePreferences(v2, version, target));
    Config expectedV2 = changed;
#define NXDK_PREF(field, low, high, ...) expectedV2.field = original.field;
#include "PreferenceGameplayFields.inc"
#undef NXDK_PREF
    expectedV2.gamemodemenusettings = original.gamemodemenusettings;
    expectedV2.modeGoals = original.modeGoals;
    expectedV2.assetNames = original.assetNames;
    for (size_t i = 0; i < 4; ++i) expectedV2.randomskin[i] = original.randomskin[i];
    assert(target == expectedV2);
    auto v3 = good;
    v3.resize(2008);
    nxdk_save::detail::write32(v3, 0, 3);
    nxdk_save::detail::write32(v3, 8, 499);
    target = original;
    assert(nxdk_save::decodePreferences(v3, version, target));
    Config expectedV3 = changed;
    expectedV3.gamemodemenusettings = original.gamemodemenusettings;
    expectedV3.modeGoals = original.modeGoals;
    expectedV3.assetNames = original.assetNames;
    for (size_t i = 0; i < 4; ++i) expectedV3.randomskin[i] = original.randomskin[i];
    assert(target == expectedV3);
    auto v4 = good;
    v4.resize(2428);
    nxdk_save::detail::write32(v4, 0, 4);
    nxdk_save::detail::write32(v4, 8, 604);
    target = original;
    assert(nxdk_save::decodePreferences(v4, version, target));
    Config expectedV4 = changed;
    expectedV4.modeGoals = original.modeGoals;
    expectedV4.assetNames = original.assetNames;
    for (size_t i = 0; i < 4; ++i) expectedV4.randomskin[i] = original.randomskin[i];
    assert(target == expectedV4);
    auto v5 = good;
    v5.resize(2516);
    nxdk_save::detail::write32(v5, 0, 5);
    nxdk_save::detail::write32(v5, 8, 626);
    target = original;
    assert(nxdk_save::decodePreferences(v5, version, target));
    Config expectedV5 = changed;
    expectedV5.assetNames = original.assetNames;
    for (size_t i = 0; i < 4; ++i) expectedV5.randomskin[i] = original.randomskin[i];
    assert(target == expectedV5);
    auto v6 = good;
    v6.erase(v6.begin() + 2532, v6.begin() + 3108);
    nxdk_save::detail::write32(v6, 0, 6);
    nxdk_save::detail::write32(v6, 8, 630);
    target = original;
    assert(nxdk_save::decodePreferences(v6, version, target) && target == changed);
    Config controls = changed;
    for (size_t i = 0; i < 4; ++i) {
        controls.playercontrol[i] = i % 3;
        controls.selectedProfiles[i] = i % 2;
        controls.inputConfiguration[i][1].iDevice = 3 - i;
        for (size_t group = 0; group < 2; ++group)
            for (size_t key = 0; key < 8; ++key) {
                controls.inputConfiguration[i][0].inputGameControls[group].keys[key] = 0x40000000 + i * 16 + group * 8 + key;
                controls.inputConfiguration[i][1].inputGameControls[group].keys[key] = (i * 16 + group * 8 + key) % 30;
            }
    }
    const auto controlBytes = nxdk_save::encodePreferences(controls, version);
    target = original;
    assert(nxdk_save::decodePreferences(controlBytes, version, target) && target == controls);
    assert(nxdk_save::detail::read32(controlBytes, 2532 + 36 * 4) == 1);
    assert(nxdk_save::detail::read32(controlBytes, 2532 + 36 * 4 + 4) == 1);
    assert(nxdk_save::detail::read32(controlBytes, 2532 + 36 * 4 + 8) == 0xffffffffu);
    for (size_t i = 0; i < 4; ++i) {
        const size_t base = 2532 + i * 144;
        assert(nxdk_save::detail::read32(controlBytes, base) == i % 3);
        assert(nxdk_save::detail::read32(controlBytes, base + 4) == i % 2);
        assert(nxdk_save::detail::read32(controlBytes, base + 8) == 0xffffffffu);
        assert(nxdk_save::detail::read32(controlBytes, base + 76) == 3 - i);
        for (size_t j = 0; j < 16; ++j) {
            assert(nxdk_save::detail::read32(controlBytes, base + 12 + j * 4) == 0x40000000 + i * 16 + j);
            assert(nxdk_save::detail::read32(controlBytes, base + 80 + j * 4) == (i * 16 + j) % 30);
        }
    }
    // Schema-7 indices import through the live dense-index -> physical-slot
    // map. Schema 8 must ignore later enumeration changes entirely.
    auto oldControls = controlBytes;
    nxdk_save::detail::write32(oldControls, 0, 7);
    target = original;
    target.legacyDeviceSlots = {2, 0, 3, 1};
    assert(nxdk_save::decodePreferences(oldControls, version, target));
    for (size_t i = 0; i < 4; ++i)
        assert(target.inputConfiguration[i][1].iDevice == target.legacyDeviceSlots[3-i]);
    target = original; target.legacyDeviceSlots = {3, 2, 1, 0};
    assert(nxdk_save::decodePreferences(controlBytes, version, target));
    for (size_t i = 0; i < 4; ++i) assert(target.inputConfiguration[i][1].iDevice == 3-i);
    target = original; target.legacyDeviceSlots[0] = 4;
    const auto invalidMap = target;
    assert(!nxdk_save::decodePreferences(oldControls, version, target) && target == invalidMap);
    for (int invalidKey : {0xd800, 0x110000, 0x3fffffff, 0x40000200}) {
        Config badControls = controls;
        badControls.inputConfiguration[0][0].inputGameControls[0].keys[0] = invalidKey;
        assert(nxdk_save::encodePreferences(badControls, version).empty());
        auto badBytes = controlBytes;
        nxdk_save::detail::write32(badBytes, 2532 + 12, invalidKey);
        target = original;
        assert(!nxdk_save::decodePreferences(badBytes, version, target) && target == original);
    }
    assert(nxdk_save::detail::read32(good, 3108) == 11);
    assert(nxdk_save::detail::read32(good, 3112) == 9);
    for (size_t i = 0; i < 11; ++i) {
        assert(nxdk_save::detail::read32(good, 3112 + i * 13) == 9);
        const std::string expectedName = "Choice00" + std::string(1, char('A' + i));
        assert(std::string(reinterpret_cast<const char *>(good.data() + 3116 + i * 13), 9) == expectedName);
    }
    auto missingAsset = good;
    const std::string removed = "RemovedXX";
    std::copy(removed.begin(), removed.end(), missingAsset.begin() + 3116);
    target = original;
    assert(nxdk_save::decodePreferences(missingAsset, version, target));
    Config expectedMissing = changed;
    expectedMissing.assetNames[0] = "Classic";
    expectedMissing.assetFallback[0] = true;
    assert(target == expectedMissing);
    target = original;
    std::swap(target.assetChoices[0][0], target.assetChoices[0][1]);
    assert(nxdk_save::decodePreferences(good, version, target));
    assert(target.assetNames == changed.assetNames && target.assetChoices[0][0] == "Choice00A");
    auto lowercase = good;
    lowercase[3116] = 'c';
    target = original;
    assert(nxdk_save::decodePreferences(lowercase, version, target) && target == changed);
    for (uint32_t length : {0u, 256u, 0xffffffffu}) {
        auto malformed = good;
        nxdk_save::detail::write32(malformed, 3112, length);
        target = original;
        assert(!nxdk_save::decodePreferences(malformed, version, target) && target == original);
    }
    for (uint8_t illegal : {uint8_t(0), uint8_t('/'), uint8_t('\\'), uint8_t(':')}) {
        auto malformed = good;
        malformed[3116] = illegal;
        target = original;
        assert(!nxdk_save::decodePreferences(malformed, version, target) && target == original);
    }
    auto trailing = good; trailing.push_back(0);
    target = original;
    assert(!nxdk_save::decodePreferences(trailing, version, target) && target == original);
    Config longest = changed;
    longest.assetNames[0] = std::string(255, 'x');
    longest.assetChoices[0].push_back(longest.assetNames[0]);
    const auto longRecord = nxdk_save::encodePreferences(longest, version);
    assert(!longRecord.empty());
    target = original; target.assetChoices[0] = longest.assetChoices[0];
    assert(nxdk_save::decodePreferences(longRecord, version, target) && target == longest);
    longest.assetNames[0].push_back('x');
    longest.assetChoices[0].push_back(longest.assetNames[0]);
    assert(nxdk_save::encodePreferences(longest, version).empty());
    auto invalidGoal = good;
    nxdk_save::detail::write32(invalidGoal, 2428, 6); // bounded, but absent from current mode choices
    target = original;
    assert(!nxdk_save::decodePreferences(invalidGoal, version, target) && target == original);
    Config invalid = changed;
    invalid.modeGoals[0] = 6;
    assert(nxdk_save::encodePreferences(invalid, version).empty());
    Disk goalFiles;
    goalFiles.slots[0] = nxdk_save::encode(9, good);
    goalFiles.slots[1] = nxdk_save::encode(10, invalidGoal);
    nxdk_save::PreferenceSlots goalSlots{goalFiles, original, version};
    nxdk_save::Record fallback;
    assert(nxdk_save::load(goalSlots, fallback) && fallback.generation == 9);
    auto negative = changed;
    negative.gamemodemenusettings.flag.speed = -1;
    const auto signedGood = nxdk_save::encodePreferences(negative, version);
    target = original;
    assert(nxdk_save::decodePreferences(signedGood, version, target) && target == negative);
    negative.gamemodemenusettings.flag.speed = -2;
    assert(nxdk_save::encodePreferences(negative, version).empty());
    // Reject values inside a short's range but absent from the actual menu.
    auto gap = good;
    // Appended fields are sorted: awardstyle, blueblockttl, ...
    nxdk_save::detail::write32(gap, 1832 + 4, 1); // blueblockttl: 0 or >=62
    target = original;
    assert(!nxdk_save::decodePreferences(gap, version, target) && target == original);
    target = changed;
    target.blueblockttl = 1;
    assert(nxdk_save::encodePreferences(target, version).empty());
    target = changed;
    target.allPowerupPresets[16][25] = 11;
    assert(nxdk_save::encodePreferences(target, version).empty());
    target = changed;
    target.poweruppreset = 17;
    assert(nxdk_save::encodePreferences(target, version).empty());
    Disk files;
    files.slots[0] = nxdk_save::encode(9, nxdk_save::encodePreferences(original, version));
    auto unsupported = good;
    nxdk_save::detail::write32(unsupported, 0, 99);
    files.slots[1] = nxdk_save::encode(10, unsupported); // valid CRC, unsupported payload
    nxdk_save::PreferenceSlots slots{files, original, version};
    nxdk_save::Record recovered;
    assert(nxdk_save::load(slots, recovered) && recovered.generation == 9);
    const auto protectedCopy = files.slots[0];
    assert(nxdk_save::save(slots, good) && files.slots[0] == protectedCopy);
    assert(nxdk_save::load(slots, recovered) && recovered.generation == 10);
    target = original;
    assert(nxdk_save::decodePreferences(recovered.payload, version, target) && target == changed);
    puts("PASS: explicit preference schema, ranges, versioning and all-or-nothing apply");
}

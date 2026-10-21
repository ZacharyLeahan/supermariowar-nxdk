#include "Preferences.h"
#include "GameValues.h"
#include "GameMode.h"
#include "FileList.h"
#include <array>
#include "PreferencesCodec.h"
#include "SaveFileSlots.h"
#include "SaveStore.h"
#include "Version.h"
#include "sfx.h"
#include <cstdio>

extern CGameMode *gamemodes[GAMEMODE_LAST];
extern short joystickcount;
extern SDL_Joystick **joysticks;
extern SkinList *skinlist;
extern AnnouncerList *announcerlist;
extern MusicList *musiclist;
extern WorldMusicList *worldmusiclist;
extern SoundsList *soundpacklist;
extern GraphicsList *menugraphicspacklist, *worldgraphicspacklist, *gamegraphicspacklist;
namespace {
struct PreferenceSnapshot : CGameConfig {
    std::array<short, 4> selectedProfiles{};
    std::array<short, 4> legacyDeviceSlots{0, 1, 2, 3};
    std::array<short, GAMEMODE_LAST> modeGoals{};
    std::array<std::string, nxdk_save::PreferenceAssetCount> assetNames{};
    std::array<std::vector<std::string>, nxdk_save::PreferenceAssetCount> assetChoices{};
    std::array<bool, nxdk_save::PreferenceAssetCount> assetFallback{};
    // Runtime whitelist, never serialized or replaced by a save.
    std::array<std::array<short, GAMEMODE_NUM_OPTIONS>, GAMEMODE_LAST> modeGoalChoices{};
};
static_assert(GAMEMODE_LAST == nxdk_save::PreferenceGoalCount);
bool capture(const CGameConfig &config, PreferenceSnapshot &snapshot)
{
    static_cast<CGameConfig &>(snapshot) = config;
    for (short index = 0; index < 4 && index < joystickcount; ++index) {
        if (!joysticks[index]) continue;
        const int slot = nxdk_joystick_slot(SDL_JoystickInstanceID(joysticks[index]));
        if (slot >= 0) snapshot.legacyDeviceSlots[index] = slot;
    }
    for (size_t i = 0; i < 4; ++i) {
        const auto *selected = config.playerInput.inputControls[i];
        if (selected == &config.inputConfiguration[i][0]) snapshot.selectedProfiles[i] = 0;
        else if (selected == &config.inputConfiguration[i][1]) snapshot.selectedProfiles[i] = 1;
        else return false;
    }
    for (size_t i = 0; i < GAMEMODE_LAST; ++i) {
        if (!gamemodes[i]) return false;
        auto *mode = gamemodes[i];
        for (size_t j = 0; j < GAMEMODE_NUM_OPTIONS; ++j)
            snapshot.modeGoalChoices[i][j] = mode->GetOptions()[j].iValue;
        short goal = mode->goal;
        bool found = false;
        for (short choice : snapshot.modeGoalChoices[i]) found |= goal == choice;
        // Runtime rounds/tours can use off-menu goals; save their nearest menu
        // representation without changing the active match (1000 caps to 999).
        if (!found) goal = mode->GetClosestGoal(goal);
        snapshot.modeGoals[i] = goal;
    }
    SimpleFileList *lists[] = {announcerlist, nullptr, nullptr, soundpacklist,
        menugraphicspacklist, worldgraphicspacklist, gamegraphicspacklist};
    for (size_t i = 0; i < 7; ++i) {
        size_t selected;
        if (i == 1) {
            for (size_t j = 0; j < musiclist->count(); ++j) snapshot.assetChoices[i].push_back(musiclist->nameAt(j));
            selected = musiclist->currentIndex();
        } else if (i == 2) {
            for (size_t j = 0; j < worldmusiclist->count(); ++j) snapshot.assetChoices[i].push_back(worldmusiclist->nameAt(j));
            selected = worldmusiclist->currentIndex();
        } else {
            for (size_t j = 0; j < lists[i]->count(); ++j)
                snapshot.assetChoices[i].push_back(lists[i]->at(j).filename().string());
            selected = lists[i]->currentIndex();
        }
        if (selected >= snapshot.assetChoices[i].size()) return false;
        snapshot.assetNames[i] = snapshot.assetChoices[i][selected];
    }
    if (!skinlist->count()) return false;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < skinlist->count(); ++j)
            snapshot.assetChoices[7 + i].push_back(skinlist->at(j).path.filename().string());
        const short selected = config.skinids[i];
        snapshot.assetNames[7 + i] = snapshot.assetChoices[7 + i][selected >= 0 && size_t(selected) < skinlist->count() ? selected : 0];
    }
    return true;
}
void applyAssets(CGameConfig &config, const PreferenceSnapshot &snapshot)
{
    SimpleFileList *lists[] = {announcerlist, nullptr, nullptr, soundpacklist,
        menugraphicspacklist, worldgraphicspacklist, gamegraphicspacklist};
    for (size_t i = 0; i < nxdk_save::PreferenceAssetCount; ++i) {
        size_t selected = 0;
        for (; selected < snapshot.assetChoices[i].size(); ++selected)
            if (nxdk_save::assetNameEqual(snapshot.assetChoices[i][selected], snapshot.assetNames[i])) break;
        // Every name was validated against the captured live list before apply.
        if (selected >= snapshot.assetChoices[i].size()) continue;
        if (i == 1) musiclist->setCurrent(selected);
        else if (i == 2) worldmusiclist->setCurrent(selected);
        else if (i < 7) lists[i]->setCurrentIndex(selected);
        else config.skinids[i - 7] = short(selected);
        printf("[save] asset slot=%u name=%s fallback=%d\n", unsigned(i), snapshot.assetNames[i].c_str(), snapshot.assetFallback[i]);
    }
}
}
void nxdk_load_preferences(CGameConfig &config)
{
    PreferenceSnapshot snapshot;
    if (!capture(config, snapshot)) {
        fprintf(stderr, "[save] preference snapshot sources unavailable\n");
        return;
    }
    nxdk_save::FileSlots files("D:\\smw-pref");
    nxdk_save::PreferenceSlots disk{files, snapshot, GAME_VERSION.asU32()};
    nxdk_save::Record record;
    if (!nxdk_save::load(disk, record) ||
        !nxdk_save::decodePreferences(record.payload, GAME_VERSION.asU32(), snapshot)) {
        printf("[save] defaults: no valid supported preference slot\n");
        return;
    }
    if (nxdk_save::detail::read32(record.payload, 0) == 7)
        printf("[save] migrated legacy SDL-index controls through current physical-port map; absent indices retain numeric slots\n");
    config = static_cast<const CGameConfig &>(snapshot);
    // Runtime pointers must refer to the destination, never the staged copy.
    for (size_t i = 0; i < 4; ++i)
        config.playerInput.inputControls[i] = &config.inputConfiguration[i][snapshot.selectedProfiles[i]];
    config.playerInput.ResetKeys();
    printf("[save] controls loaded profiles=%d,%d,%d,%d players=%d,%d,%d,%d\n",
        snapshot.selectedProfiles[0], snapshot.selectedProfiles[1], snapshot.selectedProfiles[2], snapshot.selectedProfiles[3],
        config.playercontrol[0], config.playercontrol[1], config.playercontrol[2], config.playercontrol[3]);
    for (size_t i = 0; i < GAMEMODE_LAST; ++i) gamemodes[i]->goal = snapshot.modeGoals[i];
    applyAssets(config, snapshot);
    printf("[save] goals loaded classic=%d time=%d\n", snapshot.modeGoals[game_mode_classic], snapshot.modeGoals[game_mode_timelimit]);
    sfx_setmusicvolume(config.musicvolume);
    sfx_setsoundvolume(config.soundvolume);
    printf("[save] preferences loaded generation=%lu music=%d music_volume=%d sound=%d "
           "sound_volume=%d cpu=%d preset=%d powerup_override=%d\n",
           (unsigned long)record.generation, config.music, config.musicvolume, config.sound,
           config.soundvolume, config.cpudifficulty, config.poweruppreset, config.overridepowerupsettings);
}
void nxdk_write_preferences(const CGameConfig &config)
{
    PreferenceSnapshot snapshot;
    if (!capture(config, snapshot)) {
        fprintf(stderr, "[save] preference snapshot sources unavailable\n");
        return;
    }
    const auto payload = nxdk_save::encodePreferences(snapshot, GAME_VERSION.asU32());
    if (payload.empty()) {
        fprintf(stderr, "[save] refusing invalid preference values\n");
        return;
    }
    nxdk_save::FileSlots files("D:\\smw-pref");
    nxdk_save::PreferenceSlots disk{files, snapshot, GAME_VERSION.asU32()};
    uint32_t generation = 0;
    if (!nxdk_save::save(disk, payload, &generation)) {
        fprintf(stderr, "[save] commit/readback failed error=%lu flush_status=%ld\n",
                (unsigned long)disk.files.lastError, (long)disk.files.lastFlushStatus);
        return;
    }
    printf("[save] goals committed classic=%d time=%d\n", snapshot.modeGoals[game_mode_classic], snapshot.modeGoals[game_mode_timelimit]);
    printf("[save] preferences committed generation=%lu music_volume=%d sound_volume=%d cpu=%d preset=%d powerup_override=%d\n",
           (unsigned long)generation, config.musicvolume, config.soundvolume, config.cpudifficulty, config.poweruppreset, config.overridepowerupsettings);
}

#pragma once
#include "tera_raid.h"
#include "encounter.h"
#include "raid_calc.h"
#include "personal_table.h"
#include "location_data.h"
#include "game_type.h"
#include "swish_crypto.h"
#include <vector>
#include <string>

// Complete raid info for display
struct RaidInfo {
    TeraDetails details;
    RaidCoord coord;
    bool hasCoord;
    TeraRaidMapParent map;
    RaidContent content;
    int slotIndex;
};

class RaidReader {
public:
    // Load all static data (encounter tables, personal data, locations)
    bool loadResources(const std::string& dataDir);

    // Process a save file and extract all active raids
    bool readSave(const std::string& savePath, GameVersion version);

    // Read raids from live game memory via dmntcht (applet mode)
    bool readLive(GameVersion version);

    const std::vector<RaidInfo>& raids() const { return raids_; }
    GameProgress progress() const { return progress_; }
    uint32_t trainerID32() const { return id32_; }

private:
    PersonalTable personal_;
    LocationData locations_;

    EncounterTable paldeaStandard_;
    EncounterTable paldeaBlack_;
    EncounterTable kitakamiStandard_;
    EncounterTable kitakamiBlack_;
    EncounterTable blueberryStandard_;
    EncounterTable blueberryBlack_;

    std::vector<RaidInfo> raids_;
    GameProgress progress_ = GameProgress::Beginning;
    uint32_t id32_ = 0;

    void processSlots(const std::vector<TeraRaidDetail>& slots,
                      TeraRaidMapParent map, GameVersion version,
                      int startIndex);
};

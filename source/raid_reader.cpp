#include "raid_reader.h"
#include <cstdio>

#ifdef __SWITCH__
#include "dmnt_mem.h"
#endif

bool RaidReader::loadResources(const std::string& dir) {
    if (!personal_.load(dir + "personal_sv"))
        return false;

    locations_.load(dir + "paldea_locations.json",
                    dir + "kitakami_locations.json",
                    dir + "blueberry_locations.json");

    paldeaStandard_.loadFromFile(dir + "encounter_gem_paldea_standard.pkl", personal_);
    paldeaBlack_.loadFromFile(dir + "encounter_gem_paldea_black.pkl", personal_);
    kitakamiStandard_.loadFromFile(dir + "encounter_gem_kitakami_standard.pkl", personal_);
    kitakamiBlack_.loadFromFile(dir + "encounter_gem_kitakami_black.pkl", personal_);
    blueberryStandard_.loadFromFile(dir + "encounter_gem_blueberry_standard.pkl", personal_);
    blueberryBlack_.loadFromFile(dir + "encounter_gem_blueberry_black.pkl", personal_);

    return true;
}

bool RaidReader::readSave(const std::string& savePath, GameVersion version) {
    raids_.clear();

    // Read save file
    FILE* f = fopen(savePath.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    // Decrypt SCBlocks
    auto blocks = SwishCrypto::decrypt(data.data(), data.size());
    if (blocks.empty()) return false;

    // Extract progress and trainer ID
    progress_ = getGameProgress(blocks);
    id32_ = getTrainerID32(blocks);

    // Extract raid slots
    auto raidData = RaidBlockData::extract(blocks);

    // Process each region
    processSlots(raidData.paldea, TeraRaidMapParent::Paldea, version, 0);
    processSlots(raidData.kitakami, TeraRaidMapParent::Kitakami, version, 72);
    processSlots(raidData.blueberry, TeraRaidMapParent::Blueberry, version, 172);

    return !raids_.empty();
}

bool RaidReader::readLive([[maybe_unused]] GameVersion version) {
#ifdef __SWITCH__
    raids_.clear();

    // Read Paldea raid block from game memory
    std::vector<uint8_t> paldeaBuf(DmntPointers::KTeraRaidPaldeaSize);
    if (!DmntMem::readBlock(DmntPointers::KTeraRaidPaldea, DmntPointers::KTeraRaidPaldeaLen,
                            paldeaBuf.data(), paldeaBuf.size()))
        return false;

    // Read DLC raid block (Kitakami + Blueberry)
    std::vector<uint8_t> dlcBuf(DmntPointers::KTeraRaidDLCSize);
    if (!DmntMem::readBlock(DmntPointers::KTeraRaidDLC, DmntPointers::KTeraRaidDLCLen,
                            dlcBuf.data(), dlcBuf.size()))
        return false;

    // Read MyStatus block for trainer ID
    std::vector<uint8_t> statusBuf(DmntPointers::KMyStatusSize);
    if (DmntMem::readBlock(DmntPointers::KMyStatus, DmntPointers::KMyStatusLen,
                           statusBuf.data(), statusBuf.size())) {
        if (statusBuf.size() >= 8) {
            const uint8_t* d = statusBuf.data() + 0x04;
            id32_ = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
        }
    }

    // Assume post-game for live mode (encrypted progress flags are complex to read)
    progress_ = GameProgress::Unlocked6Stars;

    // Parse raid slots from raw bytes
    std::vector<TeraRaidDetail> paldea, kitakami, blueberry;
    RaidBlockData::parsePaldea(paldeaBuf.data(), paldeaBuf.size(), paldea);
    RaidBlockData::parseDLC(dlcBuf.data(), dlcBuf.size(), kitakami, blueberry);

    // Process each region (shared with save file path)
    processSlots(paldea, TeraRaidMapParent::Paldea, version, 0);
    processSlots(kitakami, TeraRaidMapParent::Kitakami, version, 72);
    processSlots(blueberry, TeraRaidMapParent::Blueberry, version, 172);

    return !raids_.empty();
#else
    return false;
#endif
}

void RaidReader::processSlots(const std::vector<TeraRaidDetail>& slots,
                              TeraRaidMapParent map, GameVersion version,
                              int startIndex) {
    for (int i = 0; i < (int)slots.size(); i++) {
        auto& slot = slots[i];

        if (!slot.isEnabled || slot.areaID == 0)
            continue;

        // Skip event/mighty raids (not supported in v1)
        RaidContent rc = slot.raidContent();
        if (rc == RaidContent::Event || rc == RaidContent::Event_Mighty)
            continue;

        // Select encounter table
        const std::vector<EncounterTeraTF9>* table = nullptr;
        switch (map) {
            case TeraRaidMapParent::Paldea:
                table = (rc == RaidContent::Black)
                    ? &paldeaBlack_.entries
                    : &paldeaStandard_.entries;
                break;
            case TeraRaidMapParent::Kitakami:
                table = (rc == RaidContent::Black)
                    ? &kitakamiBlack_.entries
                    : &kitakamiStandard_.entries;
                break;
            case TeraRaidMapParent::Blueberry:
                table = (rc == RaidContent::Black)
                    ? &blueberryBlack_.entries
                    : &blueberryStandard_.entries;
                break;
        }

        if (!table || table->empty())
            continue;

        // Find encounter from seed
        auto* enc = getEncounterFromSeed(slot.seed, *table, version, progress_, rc, map);
        if (!enc)
            continue;

        // Generate pokemon details
        RaidInfo info;
        info.details = RaidCalc::generateData(slot.seed, *enc, id32_, personal_);
        info.map = map;
        info.content = rc;
        info.slotIndex = startIndex + i;

        // Look up coordinates
        info.hasCoord = locations_.getCoord(map, slot.areaID, slot.lotteryGroup,
                                            slot.spawnPointID, info.coord);

        raids_.push_back(info);
    }
}

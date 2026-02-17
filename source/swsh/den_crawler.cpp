#include "swsh/den_crawler.h"
#include "swsh/den_hashes.h"
#include "swsh/sword_nests.h"
#include "swsh/shield_nests.h"
#include "xoroshiro128plus.h"
#include "swish_crypto.h"
#include "sc_block.h"
#include <cstdio>

#ifdef __SWITCH__
#include "dmnt_mem.h"
#endif

namespace {
    constexpr uint32_t KEY_RAID_GALAR = 0x9033eb7b;
    constexpr uint32_t KEY_RAID_IOA   = 0x158DA896;
    constexpr uint32_t KEY_RAID_CT    = 0x148DA703;
}

bool DenCrawler::readLive(GameVersion version) {
    dens_.clear();
    version_ = version;

#ifdef __SWITCH__
    bool ok = true;
    ok &= readRegion(SwShDenRegion::Vanilla,
                     SwShOffsets::DEN_VANILLA,
                     SwShOffsets::DEN_COUNT_VANILLA, 0);
    ok &= readRegion(SwShDenRegion::IslandOfArmor,
                     SwShOffsets::DEN_ISLAND_ARMOR,
                     SwShOffsets::DEN_COUNT_IOA, 100);
    ok &= readRegion(SwShDenRegion::CrownTundra,
                     SwShOffsets::DEN_CROWN_TUNDRA,
                     SwShOffsets::DEN_COUNT_CT, 190);
    return ok;
#else
    return false;
#endif
}

bool DenCrawler::readSave(const std::string& savePath, GameVersion version) {
    dens_.clear();
    version_ = version;

    FILE* f = fopen(savePath.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    auto blocks = SwishCrypto::decrypt(data.data(), data.size());
    if (blocks.empty()) return false;

    // Find den blocks by key
    const SCBlock* galarBlock = SwishCrypto::findBlock(blocks, KEY_RAID_GALAR);
    const SCBlock* ioaBlock   = SwishCrypto::findBlock(blocks, KEY_RAID_IOA);
    const SCBlock* ctBlock    = SwishCrypto::findBlock(blocks, KEY_RAID_CT);

    if (!galarBlock || !ioaBlock || !ctBlock)
        return false;

    bool ok = true;
    ok &= readRegionFromBuffer(SwShDenRegion::Vanilla,
                               galarBlock->data.data(), galarBlock->data.size(),
                               SwShOffsets::DEN_COUNT_VANILLA, 0);
    ok &= readRegionFromBuffer(SwShDenRegion::IslandOfArmor,
                               ioaBlock->data.data(), ioaBlock->data.size(),
                               SwShOffsets::DEN_COUNT_IOA, 100);
    ok &= readRegionFromBuffer(SwShDenRegion::CrownTundra,
                               ctBlock->data.data(), ctBlock->data.size(),
                               SwShOffsets::DEN_COUNT_CT, 190);
    return ok;
}

bool DenCrawler::readRegionFromBuffer(SwShDenRegion region, const uint8_t* data,
                                       size_t dataSize, int count, int hashIndexBase) {
    const size_t needed = count * SwShDenData::SIZE;
    if (dataSize < needed) return false;

    for (int i = 0; i < count; i++) {
        SwShDenData den{};
        std::memcpy(den.raw, data + i * SwShDenData::SIZE, SwShDenData::SIZE);

        SwShDenInfo info{};
        info.denIndex   = hashIndexBase + i;
        info.region     = region;
        info.seed       = den.seed();
        info.stars      = den.stars();
        info.isActive   = den.isActive();
        info.isRare     = den.isRare();
        info.isEvent    = den.isEvent();
        info.species    = 0;
        info.flawlessIVs = 0;
        info.shinyType  = SwShShinyType::None;
        info.shinyAdvance = 0;

        resolveEncounter(den, info.denIndex, info.species, info.flawlessIVs);

        if (!info.isActive || !info.isEvent) {
            info.shinyType = predictShiny(info.seed, 10000, info.shinyAdvance);
        }

        dens_.push_back(info);
    }
    return true;
}

bool DenCrawler::readRegion(SwShDenRegion region, uint64_t heapOffset,
                            int count, int hashIndexBase) {
#ifdef __SWITCH__
    const size_t blockSize = count * SwShDenData::SIZE;
    uint8_t buf[SwShOffsets::DEN_COUNT_VANILLA * SwShDenData::SIZE]; // largest region
    if (blockSize > sizeof(buf)) return false;

    if (!DmntMem::readHeap(heapOffset, buf, blockSize))
        return false;

    for (int i = 0; i < count; i++) {
        SwShDenData den{};
        std::memcpy(den.raw, buf + i * SwShDenData::SIZE, SwShDenData::SIZE);

        SwShDenInfo info{};
        info.denIndex   = hashIndexBase + i;
        info.region     = region;
        info.seed       = den.seed();
        info.stars      = den.stars();
        info.isActive   = den.isActive();
        info.isRare     = den.isRare();
        info.isEvent    = den.isEvent();
        info.species    = 0;
        info.flawlessIVs = 0;
        info.shinyType  = SwShShinyType::None;
        info.shinyAdvance = 0;

        resolveEncounter(den, info.denIndex, info.species, info.flawlessIVs);

        if (!info.isActive || !info.isEvent) {
            info.shinyType = predictShiny(info.seed, 10000, info.shinyAdvance);
        }

        dens_.push_back(info);
    }
    return true;
#else
    (void)region; (void)heapOffset; (void)count; (void)hashIndexBase;
    return false;
#endif
}

void DenCrawler::resolveEncounter(const SwShDenData& den, int globalIndex,
                                  uint16_t& outSpecies, uint8_t& outFlawlessIVs) const {
    if (den.isActive() && den.isEvent()) {
        outSpecies = 0;
        outFlawlessIVs = 0;
        return;
    }

    // Inactive dens always use normal (non-rare) nest table
    uint8_t nestId = SwShDenHashes::getNestId(globalIndex, den.isRare());

    const SwShDenEncounter* table;
    if (version_ == GameVersion::Sword)
        table = SwShSwordNests::getSwordEncounters(nestId);
    else
        table = SwShShieldNests::getShieldEncounters(nestId);

    // Encounter resolution: accumulate probabilities until > randRoll
    uint32_t accumulated = 1;
    uint8_t stars = den.stars();
    uint32_t randRoll = den.randRoll();

    for (int j = 0; j < 12; j++) {
        uint32_t prob = (stars < 5) ? table[j].probabilities[stars] : 0;
        accumulated += prob;

        if (accumulated > randRoll) {
            outSpecies = table[j].species;
            outFlawlessIVs = table[j].flawlessIVs;
            return;
        }
    }

    outSpecies = 0;
    outFlawlessIVs = 0;
}

SwShShinyType DenCrawler::predictShiny(uint64_t seed, uint32_t maxAdvances,
                                       uint32_t& outAdvance) {
    uint64_t currentSeed = seed;

    for (uint32_t i = 0; i < maxAdvances; i++) {
        Xoroshiro128Plus rng(currentSeed);

        // First call produces EC and also serves as next seed
        currentSeed = rng.next();

        // SID/TID and PID (truncated to 32-bit)
        uint32_t sidTid = static_cast<uint32_t>(rng.next());
        uint32_t pid    = static_cast<uint32_t>(rng.next());

        uint32_t psv = (pid >> 16) ^ (pid & 0xFFFF);
        uint32_t tsv = (sidTid >> 16) ^ (sidTid & 0xFFFF);

        if (psv == tsv) {
            outAdvance = i + 1;
            return SwShShinyType::Square;
        }
        if ((psv ^ tsv) < 16) {
            outAdvance = i + 1;
            return SwShShinyType::Star;
        }
    }

    outAdvance = 0;
    return SwShShinyType::None;
}

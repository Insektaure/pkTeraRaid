#pragma once
#include "swsh/den_types.h"
#include "game_type.h"
#include <vector>
#include <string>

class DenCrawler {
public:
    // Read all 276 dens from live game memory via dmntcht
    bool readLive(GameVersion version);

    // Read all 276 dens from a decrypted save file
    bool readSave(const std::string& savePath, GameVersion version);

    const std::vector<SwShDenInfo>& dens() const { return dens_; }

    // Predict the nearest shiny advance for a given seed.
    // Returns the shiny type and sets outAdvance to the advance count (1-based).
    // If no shiny found within maxAdvances, returns None and outAdvance = 0.
    static SwShShinyType predictShiny(uint64_t seed, uint32_t maxAdvances, uint32_t& outAdvance);

private:
    std::vector<SwShDenInfo> dens_;
    GameVersion version_ = GameVersion::Sword;

    bool readRegion(SwShDenRegion region, uint64_t heapOffset, int count, int hashIndexBase);
    bool readRegionFromBuffer(SwShDenRegion region, const uint8_t* data, size_t dataSize,
                              int count, int hashIndexBase);

    // Resolve species + flawlessIVs for a den using encounter tables
    void resolveEncounter(const SwShDenData& den, int globalIndex,
                          uint16_t& outSpecies, uint8_t& outFlawlessIVs) const;
};

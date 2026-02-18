#pragma once
#include "tera_raid.h"
#include "xoroshiro128plus.h"
#include "game_type.h"
#include <cstdint>
#include <vector>
#include <string>

// GemType - Tera type specification
enum class GemType : uint8_t {
    Default = 0, // Use PersonalInfo type1/type2
    Random = 1,  // Random from 18 types
    // 2-19 are specific types (type = gemType - 2)
};

inline bool gemTypeIsSpecified(GemType gem, uint8_t& outType) {
    auto v = (uint8_t)gem;
    if (v >= 2) {
        outType = v - 2;
        return true;
    }
    return false;
}

// AbilityPermission
enum class AbilityPermission : uint8_t {
    Any12 = 0,
    Any12H = 1,
    OnlyFirst = 2,
    OnlySecond = 4,
    OnlyHidden = 8,
};

// Shiny type from encounter definition
enum class ShinyType : uint8_t {
    Random = 0,
    Never = 1,
    Always = 2,
};

// SizeType for scale
enum class SizeType9 : uint8_t {
    RANDOM = 0,
    XS = 1,
    S = 2,
    M = 3,
    L = 4,
    XL = 5,
    VALUE = 6,
};

// Standard encounter from .pkl file (0x3C per entry)
struct EncounterTeraTF9 {
    static constexpr int SERIALIZED_SIZE = 0x3C;

    uint16_t species;
    uint8_t form;
    uint8_t gender;        // 0-indexed (raw - 1)
    AbilityPermission ability;
    uint8_t flawlessIVCount;
    ShinyType shiny;
    uint8_t level;
    uint16_t moves[4];
    GemType teraType;
    uint8_t index;         // 0=standard, >0=distribution
    uint8_t stars;
    uint8_t randRate;
    int16_t randRateMinScarlet;
    int16_t randRateMinViolet;
    uint32_t identifier;
    uint64_t fixedRewardHash;
    uint64_t lotteryRewardHash;
    uint16_t extraMoves[6];
    uint8_t genderRatio;   // from PersonalInfo

    static EncounterTeraTF9 readFrom(const uint8_t* data, uint8_t personalGender);
};

// Encounter table for a map+content combination
struct EncounterTable {
    std::vector<EncounterTeraTF9> entries;
    TeraRaidMapParent map;

    bool loadFromFile(const std::string& path, const class PersonalTable& pt);
};

// Rate totals from EncounterTera9.cs (PKHeX)
namespace RateTotals {
    inline int16_t paldeaSL(int stars) {
        constexpr int16_t t[] = {0, 5800, 5300, 7400, 8800, 9100, 6500};
        return (stars >= 1 && stars <= 6) ? t[stars] : 0;
    }
    inline int16_t paldeaVL(int stars) {
        constexpr int16_t t[] = {0, 5800, 5300, 7400, 8700, 9100, 6500};
        return (stars >= 1 && stars <= 6) ? t[stars] : 0;
    }
    inline int16_t kitakamiSL(int stars) {
        constexpr int16_t t[] = {0, 1500, 1500, 2500, 2100, 2250, 2475};
        return (stars >= 1 && stars <= 6) ? t[stars] : 0;
    }
    inline int16_t kitakamiVL(int stars) {
        constexpr int16_t t[] = {0, 1500, 1500, 2500, 2100, 2250, 2574};
        return (stars >= 1 && stars <= 6) ? t[stars] : 0;
    }
    inline int16_t blueberry(int stars) {
        constexpr int16_t t[] = {0, 1100, 1100, 2000, 1900, 2100, 2600};
        return (stars >= 1 && stars <= 6) ? t[stars] : 0;
    }

    inline int16_t getTotal(int stars, TeraRaidMapParent map, GameVersion ver) {
        if (ver == GameVersion::Scarlet) {
            switch (map) {
                case TeraRaidMapParent::Paldea:    return paldeaSL(stars);
                case TeraRaidMapParent::Kitakami:   return kitakamiSL(stars);
                case TeraRaidMapParent::Blueberry:  return blueberry(stars);
            }
        } else {
            switch (map) {
                case TeraRaidMapParent::Paldea:    return paldeaVL(stars);
                case TeraRaidMapParent::Kitakami:   return kitakamiVL(stars);
                case TeraRaidMapParent::Blueberry:  return blueberry(stars);
            }
        }
        return 0;
    }
}

// Star selection from seed based on game progress
// From EncounterTeraTF9::GetSeedStars()
inline uint8_t getSeedStars(Xoroshiro128Plus& xoro, GameProgress progress) {
    auto rand = xoro.nextInt(100);
    switch (progress) {
        case GameProgress::Unlocked6Stars:
            if (rand > 70) return 5;
            if (rand > 30) return 4;
            return 3;
        case GameProgress::Unlocked5Stars:
            if (rand > 75) return 5;
            if (rand > 40) return 4;
            return 3;
        case GameProgress::Unlocked4Stars:
            if (rand > 70) return 4;
            if (rand > 40) return 3;
            if (rand > 20) return 2;
            return 1;
        case GameProgress::Unlocked3Stars:
            if (rand > 70) return 3;
            if (rand > 30) return 2;
            return 1;
        default:
            if (rand > 80) return 2;
            return 1;
    }
}

// Find encounter from seed for standard/black raids
// From EncounterTeraTF9::GetEncounterFromSeed()
const EncounterTeraTF9* getEncounterFromSeed(
    uint32_t seed,
    const std::vector<EncounterTeraTF9>& encounters,
    GameVersion version,
    GameProgress progress,
    RaidContent content,
    TeraRaidMapParent map);

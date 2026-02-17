#pragma once
#include <cstdint>
#include <cstring>

enum class SwShDenRegion : uint8_t {
    Vanilla       = 0,  // 100 dens, global indices 0-99
    IslandOfArmor = 1,  //  90 dens, global indices 100-189
    CrownTundra   = 2,  //  86 dens, global indices 190-275
};

namespace SwShOffsets {
    // Heap offsets for den data blocks
    constexpr uint64_t DEN_VANILLA      = 0x450c8a70;
    constexpr uint64_t DEN_ISLAND_ARMOR = 0x450c94d8;
    constexpr uint64_t DEN_CROWN_TUNDRA = 0x450c9f40;

    constexpr int DEN_COUNT_VANILLA = 100;
    constexpr int DEN_COUNT_IOA     =  90;
    constexpr int DEN_COUNT_CT      =  86;
    constexpr int DEN_COUNT_TOTAL   = 276;
    constexpr int DEN_SIZE          = 0x18;
}

// Single encounter entry within a nest table (12 per nest)
struct SwShDenEncounter {
    uint16_t species;          // National dex number
    uint8_t  flawlessIVs;
    uint32_t probabilities[5]; // Per star level (indices 0-4 = 1-5 stars)
};

// Raw den from game memory (0x18 bytes)
struct SwShDenData {
    static constexpr int SIZE = 0x18;

    uint8_t raw[SIZE];

    uint64_t seed() const {
        uint64_t v;
        std::memcpy(&v, raw + 0x08, 8);
        return v;
    }

    uint8_t stars() const {
        uint8_t s = raw[0x10];
        return s > 4 ? 4 : s;
    }

    uint8_t randRoll() const { return raw[0x11]; }
    uint8_t denType() const  { return raw[0x12]; }
    uint8_t flagByte() const { return raw[0x13]; }

    bool isActive() const { return denType() > 0; }
    bool isRare() const   { return isActive() && (denType() & 1) == 0; }
    bool isEvent() const  { return ((flagByte() >> 1) & 1) == 1; }
};

enum class SwShShinyType : uint8_t {
    None   = 0,
    Star   = 1,
    Square = 2,
};

// Complete resolved den information for display
struct SwShDenInfo {
    int           denIndex;       // 0-275
    SwShDenRegion region;
    uint64_t      seed;
    uint8_t       stars;          // 0-4 (display as 1-5)
    bool          isActive;
    bool          isRare;
    bool          isEvent;
    uint16_t      species;        // National dex number, 0 if event/unresolved
    uint8_t       flawlessIVs;
    int           ivs[6];         // HP, Atk, Def, SpA, SpD, Spe
    SwShShinyType shinyType;
    uint32_t      shinyAdvance;   // 0 = no shiny found within search range
};

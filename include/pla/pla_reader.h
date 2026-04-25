#pragma once
#include "pla/pla_encounter.h"
#include "pla/pla_region.h"
#include "game_type.h"
#include <cstdint>
#include <vector>
#include <string>

// PLA pointers (game versions 1.1.0 and 1.1.1 - chains from
// Lincoln-LM/PLA-Live-Map main.py @ 64ce79d, verified unchanged on 1.1.1)
// SPAWNER_PTR  = [[main+42a6ee0]+330]           (spawner array base)
// OUTBREAK_PTR = [[[[main+42BA6B0]+2B0]+58]+18] (outbreak info)
// PARTY_PTR    = [[[main+42a7000]+d0]+58]
// WILD_PTR     = [[[[main+42a6f00]+b0]+e0]+d0]
namespace PlaPointers {
    // Spawner array base chain (stops before final deref -> returns the address).
    // Two strides exist:
    //  - Position list:  base + 0x70 + idx*0x40   (pos 12B, seed 12B at +0x90+idx*0x40)
    //  - Group data:     base + 0x70 + gid*0x440  (generator seed 8B at +0x20)
    constexpr uint64_t SPAWNER_BASE[] = {0x42A6EE0, 0x330};
    constexpr int      SPAWNER_BASE_LEN = 2;

    constexpr uint64_t OUTBREAK_BASE[] = {0x42BA6B0, 0x2B0, 0x58, 0x18};
    constexpr int      OUTBREAK_BASE_LEN = 4;

    // Player world position — writable. Chain ends at 12 bytes of {float x, y, z}.
    // [[[[[[main+42F18E8]+88]+90]+1F0]+18]+80]+90
    constexpr uint64_t PLAYER_POS_CHAIN[] = {0x42F18E8, 0x88, 0x90, 0x1F0, 0x18, 0x80, 0x90};
    constexpr int      PLAYER_POS_CHAIN_LEN = 7;

    constexpr uint64_t GROUP_STRIDE      = 0x440;
    constexpr uint64_t GROUP_GEN_SEED    = 0x20;     // generator_seed within group struct
    constexpr uint64_t GROUP_ACTIVE_SEED = 0x408;    // non-zero when outbreak group is live

    constexpr uint64_t POS_STRIDE        = 0x40;
    constexpr uint64_t POS_POSITION      = 0x70;
    constexpr uint64_t POS_SEED          = 0x90;

    constexpr uint64_t SIZE_FIELD        = 0x18;     // size of 0x40-stride list
}

struct PlaSpawner {
    // Raw (region-independent)
    int       groupId;
    uint64_t  generatorSeed;
    uint64_t  groupSeed;           // generatorSeed - XOROSHIRO_CONST

    // Decorated (filled by decorate() - depend on the selected PlaRegion)
    int       region;              // PlaRegion or -1 if no marker in selected region
    float     markerX, markerY, markerZ;
    uint8_t   guaranteedIvs;
    bool      active;              // marker matches a live-position entry in the selected region
    float     livePosX, livePosY, livePosZ;
    PlaEncounter::Fixed firstSpawn;
    int       shinyAdvance;
    const char* speciesName;
    uint16_t    speciesId;   // national dex id; 0 if unresolved
    uint8_t     form;        // form index for sprite lookup
    bool        alpha;
};

struct PlaOutbreak {
    bool     present;
    int      groupId;
    uint64_t groupSeed;
    uint8_t  spawnCount;           // 10..15 typical
};

class PlaReader {
public:
    // Read all groups with non-zero seeds + scan the runtime live-position list.
    // Region-independent - group_ids are region-local and only meaningful once
    // decorate() is called with the correct region.
    bool readLive(int maxGroups = 256);

    // Re-interpret the raw spawner list against a specific region's markers.
    // Populates species/shiny/active fields. Cheap: just per-spawner RNG, no I/O.
    void decorate(PlaRegion region, int shinyRolls = 1, int maxAdvance = 5000);

    PlaOutbreak readOutbreak();

    // Write the player's world position. Returns true on success.
    static bool teleport(float x, float y, float z);

    const std::vector<PlaSpawner>& spawners() const { return spawners_; }

private:
    std::vector<PlaSpawner> spawners_;
    struct LivePos { float x, y, z; };
    std::vector<LivePos> livePositions_;
};

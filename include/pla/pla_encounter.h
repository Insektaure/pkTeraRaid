#pragma once
#include "xoroshiro128plus.h"
#include <cstdint>

// Port of PLA-Live-Map's generate_from_seed (main.py:80) and next_filtered (main.py:150).
// RNG layout reference: https://github.com/Lincoln-LM/PLA-Live-Map
namespace PlaEncounter {

struct Fixed {
    uint32_t ec;
    uint32_t pid;
    int      ivs[6];    // 0..31, -1 means unset
    uint8_t  nature;    // 0..24
    bool     shiny;
};

// Generate a fixed-seed Pokemon (init spawn or respawn): EC, SIDTID, PID (with shiny rerolls),
// 6 IVs (with `guaranteedIvs` of them set to 31), ability, gender, nature.
inline Fixed generateFromSeed(uint64_t seed, int rolls, int guaranteedIvs) {
    Xoroshiro128Plus rng(seed);
    Fixed f{};
    f.ec     = static_cast<uint32_t>(rng.nextInt(0xFFFFFFFFULL));
    uint32_t sidtid = static_cast<uint32_t>(rng.nextInt(0xFFFFFFFFULL));
    f.shiny = false;
    for (int i = 0; i < rolls; i++) {
        f.pid = static_cast<uint32_t>(rng.nextInt(0xFFFFFFFFULL));
        uint32_t x = (f.pid >> 16) ^ (sidtid >> 16) ^ (f.pid & 0xFFFF) ^ (sidtid & 0xFFFF);
        if (x < 0x10) { f.shiny = true; break; }
    }
    for (int i = 0; i < 6; i++) f.ivs[i] = -1;
    for (int i = 0; i < guaranteedIvs; i++) {
        int idx = (int)rng.nextInt(6);
        while (f.ivs[idx] != -1) idx = (int)rng.nextInt(6);
        f.ivs[idx] = 31;
    }
    for (int i = 0; i < 6; i++) {
        if (f.ivs[i] == -1) f.ivs[i] = (int)rng.nextInt(32);
    }
    // Positional RNG reads - ability slot (0/1) and gender roll (1..252) would
    // go here. We don't display them (gender needs per-species ratio we don't
    // need to ship), but the rolls must still be consumed so `nature` stays in sync.
    rng.nextInt(2);
    rng.nextInt(252);
    f.nature = (uint8_t)rng.nextInt(25);
    return f;
}

// Advance the group RNG and search for the first advance whose init spawn is shiny.
// Returns advance count (0 = current). -1 if none within `maxAdvance`.
// initSpawn=true means: the current generator_seed is the initial spawn; don't advance first.
inline int nextShiny(uint64_t groupSeed, int rolls, int guaranteedIvs,
                     bool initSpawn, int maxAdvance) {
    Xoroshiro128Plus main(groupSeed);
    if (!initSpawn) {
        main.next();                        // spawner 0
        main.next();                        // spawner 1
        uint64_t r = main.next();
        main.s0 = r; main.s1 = Xoroshiro128Plus::XOROSHIRO_CONST;
    }
    for (int adv = 0; adv <= maxAdvance; adv++) {
        uint64_t gen = main.next();
        main.next();                        // spawner 1's seed, unused
        Xoroshiro128Plus spawner(gen);
        spawner.next();                     // slot roll, unused
        uint64_t fixedSeed = spawner.next();
        Fixed f = generateFromSeed(fixedSeed, rolls, guaranteedIvs);
        if (f.shiny) return adv;
        uint64_t r = main.next();
        main.s0 = r; main.s1 = Xoroshiro128Plus::XOROSHIRO_CONST;
    }
    return -1;
}

// Compute the init-spawn details from a group seed (no advance).
// If outSlotRoll is non-null, stores the slot roll in [0, 1) — multiply by slotTotal.
inline Fixed firstFixed(uint64_t groupSeed, int rolls, int guaranteedIvs,
                        double* outSlotRoll = nullptr) {
    Xoroshiro128Plus main(groupSeed);
    uint64_t r = main.next();
    main.s0 = r; main.s1 = Xoroshiro128Plus::XOROSHIRO_CONST;
    uint64_t slotRaw = main.next();
    if (outSlotRoll) *outSlotRoll = (double)slotRaw / 18446744073709551616.0; // 2^64
    uint64_t fixedSeed = main.next();
    return generateFromSeed(fixedSeed, rolls, guaranteedIvs);
}

} // namespace PlaEncounter

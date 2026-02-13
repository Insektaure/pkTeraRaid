#pragma once
#include "encounter.h"
#include "xoroshiro128plus.h"
#include "shiny_util.h"
#include "personal_table.h"
#include <cstdint>

// TeraShiny matches TeraFinder.Core enum values
enum class TeraShiny : int {
    Any = 0,
    No = 1,
    Yes = 2,
    Star = 3,
    Square = 4,
};

// Gender enum
enum class Gender : uint8_t {
    Male = 0,
    Female = 1,
    Genderless = 2,
};

// Full raid details generated from seed + encounter
struct TeraDetails {
    uint32_t seed;
    TeraShiny shiny;
    uint8_t stars;
    uint16_t species;
    uint8_t form;
    uint8_t level;
    uint8_t teraType;
    uint32_t EC;
    uint32_t PID;
    int ivs[6]; // HP, ATK, DEF, SPA, SPD, SPE
    int ability;
    int abilityNumber;
    uint8_t nature;
    Gender gender;
    uint8_t height;
    uint8_t weight;
    uint8_t scale;
    uint16_t moves[4];
};

namespace RaidCalc {

// Get tera type from seed and encounter specification
uint8_t getTeraType(uint64_t seed, GemType gem, uint16_t species, uint8_t form, const PersonalTable& pt);

// Generate full pokemon details from seed + encounter
TeraDetails generateData(uint32_t seed, const EncounterTeraTF9& encounter, uint32_t id32, const PersonalTable& pt);

} // namespace RaidCalc

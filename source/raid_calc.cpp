#include "raid_calc.h"

namespace RaidCalc {

uint8_t getTeraType(uint64_t seed, GemType gem, uint16_t species, uint8_t form, const PersonalTable& pt) {
    uint8_t specifiedType;
    if (gemTypeIsSpecified(gem, specifiedType))
        return specifiedType;

    auto rand = Xoroshiro128Plus(seed);
    if (gem == GemType::Random)
        return (uint8_t)rand.nextInt(18);

    // Default: pick type1 or type2 based on pivot
    auto pivot = rand.nextInt(2);
    auto pi = pt.getFormEntry(species, form);
    return pivot == 0 ? pi.type1() : pi.type2();
}

static Gender getGender(uint8_t ratio, uint64_t rand100) {
    switch (ratio) {
        case 0x1F: return rand100 < 12 ? Gender::Female : Gender::Male;
        case 0x3F: return rand100 < 25 ? Gender::Female : Gender::Male;
        case 0x7F: return rand100 < 50 ? Gender::Female : Gender::Male;
        case 0xBF: return rand100 < 75 ? Gender::Female : Gender::Male;
        case 0xE1: return rand100 < 89 ? Gender::Female : Gender::Male;
        default:   return rand100 < 50 ? Gender::Female : Gender::Male;
    }
}

// Toxtricity nature table (Low Key form)
static uint8_t getToxNatureLowKey(Xoroshiro128Plus& rand) {
    // Low Key natures: Lonely, Bold, Relaxed, Timid, Serious, Modest,
    // Mild, Quiet, Bashful, Calm, Gentle, Careful
    constexpr uint8_t lowKey[] = {1,2,5,10,12,15,16,17,18,20,21,23};
    return lowKey[rand.nextInt(12)];
}

static uint8_t getToxNatureAmpedUp(Xoroshiro128Plus& rand) {
    // Amped Up natures: Hardy, Brave, Adamant, Naughty, Docile,
    // Impish, Lax, Hasty, Jolly, Naive, Rash, Sassy, Quirky
    constexpr uint8_t amped[] = {0,3,4,6,7,8,9,11,13,14,16,19,22,24};
    return amped[rand.nextInt(14)];
}

static int getRefreshedAbility(const PersonalTable& pt, uint16_t species, uint8_t form, int abilNum) {
    auto pi = pt.getFormEntry(species, form);
    int idx = abilNum >> 1;
    if ((uint32_t)idx < (uint32_t)pi.abilityCount())
        return pi.getAbilityAtIndex(idx);
    return idx;
}

TeraDetails generateData(uint32_t seed, const EncounterTeraTF9& encounter, uint32_t id32, const PersonalTable& pt) {
    TeraDetails result{};
    result.seed = seed;
    result.stars = encounter.stars;
    result.species = encounter.species;
    result.form = encounter.form;
    result.level = encounter.level;
    result.moves[0] = encounter.moves[0];
    result.moves[1] = encounter.moves[1];
    result.moves[2] = encounter.moves[2];
    result.moves[3] = encounter.moves[3];

    // 1. Tera Type
    result.teraType = getTeraType(seed, encounter.teraType, encounter.species, encounter.form, pt);

    // 2. RNG sequence
    auto rand = Xoroshiro128Plus(seed);

    // EC
    result.EC = (uint32_t)rand.nextInt((uint64_t)UINT32_MAX);

    // PID + Shiny logic
    uint32_t fakeTID = (uint32_t)rand.nextInt();
    uint32_t pid = (uint32_t)rand.nextInt();

    if (encounter.shiny == ShinyType::Random) {
        auto xor_val = ShinyUtil::getShinyXor(pid, fakeTID);
        if (xor_val < 16) {
            if (xor_val != 0) xor_val = 1;
            ShinyUtil::forceShinyState(true, pid, id32, xor_val);
            result.shiny = (xor_val == 0) ? TeraShiny::Square : TeraShiny::Star;
        } else {
            ShinyUtil::forceShinyState(false, pid, id32, xor_val);
            result.shiny = TeraShiny::No;
        }
    } else if (encounter.shiny == ShinyType::Always) {
        uint16_t tid16 = (uint16_t)fakeTID;
        uint16_t sid16 = (uint16_t)(fakeTID >> 16);
        auto xor_val = ShinyUtil::getShinyXor(pid, fakeTID);
        if (xor_val > 16)
            pid = ShinyUtil::getShinyPID(tid16, sid16, pid, 0);
        if (!ShinyUtil::getIsShiny(id32, pid)) {
            xor_val = ShinyUtil::getShinyXor(pid, fakeTID);
            pid = ShinyUtil::getShinyPID(
                (uint16_t)(id32 & 0xFFFF),
                (uint16_t)(id32 >> 16),
                pid, xor_val == 0 ? 0u : 1u);
        }
        xor_val = ShinyUtil::getShinyXor(pid, fakeTID);
        result.shiny = (xor_val == 0) ? TeraShiny::Square : TeraShiny::Star;
    } else { // Never
        if (ShinyUtil::getIsShiny(fakeTID, pid))
            pid ^= 0x10000000;
        if (ShinyUtil::getIsShiny(id32, pid))
            pid ^= 0x10000000;
        result.shiny = TeraShiny::No;
    }
    result.PID = pid;

    // 3. IVs
    constexpr int UNSET = -1;
    constexpr int MAX = 31;
    for (int i = 0; i < 6; i++)
        result.ivs[i] = UNSET;

    for (int i = 0; i < encounter.flawlessIVCount; i++) {
        int index;
        do { index = (int)rand.nextInt(6); }
        while (result.ivs[index] != UNSET);
        result.ivs[index] = MAX;
    }

    for (int i = 0; i < 6; i++) {
        if (result.ivs[i] == UNSET)
            result.ivs[i] = (int)rand.nextInt(MAX + 1);
    }

    // 4. Ability
    int abilNum;
    switch (encounter.ability) {
        case AbilityPermission::Any12H:
            abilNum = (int)rand.nextInt(3) << 1;
            break;
        case AbilityPermission::Any12:
            abilNum = (int)rand.nextInt(2) << 1;
            break;
        default:
            abilNum = (int)encounter.ability;
            break;
    }
    result.ability = getRefreshedAbility(pt, encounter.species, encounter.form, abilNum);
    result.abilityNumber = (abilNum == 0) ? 1 : abilNum;

    // 5. Gender
    uint8_t genderRatio = encounter.genderRatio;
    if (genderRatio == PersonalInfo9SV::RatioMagicGenderless) {
        result.gender = Gender::Genderless;
    } else if (genderRatio == PersonalInfo9SV::RatioMagicFemale) {
        result.gender = Gender::Female;
    } else if (genderRatio == PersonalInfo9SV::RatioMagicMale) {
        result.gender = Gender::Male;
    } else {
        result.gender = getGender(genderRatio, rand.nextInt(100));
    }

    // 6. Nature
    // Toxtricity species ID = 849
    if (encounter.species == 849) {
        if (encounter.form == 0)
            result.nature = getToxNatureAmpedUp(rand);
        else
            result.nature = getToxNatureLowKey(rand);
    } else {
        result.nature = (uint8_t)rand.nextInt(25);
    }

    // 7. Height, Weight, Scale
    result.height = (uint8_t)(rand.nextInt(0x81) + rand.nextInt(0x80));
    result.weight = (uint8_t)(rand.nextInt(0x81) + rand.nextInt(0x80));
    result.scale = (uint8_t)(rand.nextInt(0x81) + rand.nextInt(0x80));

    return result;
}

} // namespace RaidCalc

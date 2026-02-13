#include "encounter.h"
#include "personal_table.h"
#include <cstdio>
#include <cstring>

EncounterTeraTF9 EncounterTeraTF9::readFrom(const uint8_t* data, uint8_t personalGender) {
    auto r16 = [](const uint8_t* p) -> uint16_t { return p[0] | (p[1] << 8); };
    auto r16s = [](const uint8_t* p) -> int16_t { return (int16_t)(p[0] | (p[1] << 8)); };
    auto r32 = [](const uint8_t* p) -> uint32_t { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); };

    EncounterTeraTF9 e;
    e.species           = r16(data + 0x00);
    e.form              = data[0x02];
    e.gender            = data[0x03] > 0 ? data[0x03] - 1 : 0;
    e.ability           = static_cast<AbilityPermission>(
        data[0x04] == 0 ? 0 :
        data[0x04] == 1 ? 1 :
        data[0x04] == 2 ? 2 :
        data[0x04] == 3 ? 4 :
        data[0x04] == 4 ? 8 : 0);
    e.flawlessIVCount   = data[0x05];
    e.shiny             = (ShinyType)data[0x06];
    e.level             = data[0x07];
    e.moves[0]          = r16(data + 0x08);
    e.moves[1]          = r16(data + 0x0A);
    e.moves[2]          = r16(data + 0x0C);
    e.moves[3]          = r16(data + 0x0E);
    e.teraType          = (GemType)data[0x10];
    e.index             = data[0x11];
    e.stars             = data[0x12];
    e.randRate          = data[0x13];
    e.randRateMinScarlet = r16s(data + 0x14);
    e.randRateMinViolet  = r16s(data + 0x16);
    e.identifier        = r32(data + 0x18);
    e.extraMoves[0]     = r16(data + 0x30);
    e.extraMoves[1]     = r16(data + 0x32);
    e.extraMoves[2]     = r16(data + 0x34);
    e.extraMoves[3]     = r16(data + 0x36);
    e.extraMoves[4]     = r16(data + 0x38);
    e.extraMoves[5]     = r16(data + 0x3A);
    e.genderRatio       = personalGender;

    return e;
}

bool EncounterTable::loadFromFile(const std::string& path, const PersonalTable& pt) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    int count = (int)(size / EncounterTeraTF9::SERIALIZED_SIZE);
    entries.clear();
    entries.reserve(count);

    for (int i = 0; i < count; i++) {
        const uint8_t* ptr = data.data() + i * EncounterTeraTF9::SERIALIZED_SIZE;
        uint16_t species = ptr[0] | (ptr[1] << 8);
        uint8_t form = ptr[0x02];
        auto pi = pt.getFormEntry(species, form);
        entries.push_back(EncounterTeraTF9::readFrom(ptr, pi.gender()));
    }

    return !entries.empty();
}

const EncounterTeraTF9* getEncounterFromSeed(
    uint32_t seed,
    const std::vector<EncounterTeraTF9>& encounters,
    GameVersion version,
    GameProgress progress,
    RaidContent content,
    TeraRaidMapParent map)
{
    auto xoro = Xoroshiro128Plus(seed);

    // Standard raids: determine star count from progress
    uint8_t randStars;
    if (content == RaidContent::Standard) {
        randStars = getSeedStars(xoro, progress);
    } else {
        randStars = 6; // Black raids are always 6-star
    }

    // Get rate total for this star/map/version
    int16_t maxRate = RateTotals::getTotal(randStars, map, version);
    if (maxRate <= 0) return nullptr;

    auto rateRand = (int)xoro.nextInt((uint64_t)maxRate);

    for (auto& enc : encounters) {
        if (enc.stars != randStars)
            continue;

        int16_t minRate = (version == GameVersion::Scarlet)
            ? enc.randRateMinScarlet
            : enc.randRateMinViolet;

        if (minRate < 0)
            continue;

        if ((uint32_t)(rateRand - minRate) < enc.randRate)
            return &enc;
    }

    return nullptr;
}

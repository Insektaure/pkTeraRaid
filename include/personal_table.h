#pragma once
#include <cstdint>
#include <vector>
#include <string>

// PersonalInfo9SV - ported from PKHeX.Core
// Binary format: 0x50 bytes per entry
struct PersonalInfo9SV {
    const uint8_t* data;

    uint8_t type1()         const { return data[0x06]; }
    uint8_t type2()         const { return data[0x07]; }
    uint8_t gender()        const { return data[0x0C]; }
    uint16_t ability1()     const { return data[0x12] | (data[0x13] << 8); }
    uint16_t ability2()     const { return data[0x14] | (data[0x15] << 8); }
    uint16_t abilityH()     const { return data[0x16] | (data[0x17] << 8); }
    uint16_t formStatsIndex() const { return data[0x18] | (data[0x19] << 8); }
    uint8_t formCount()     const { return data[0x1A]; }
    bool isPresentInGame()  const { return data[0x1C] != 0; }

    int getAbilityAtIndex(int index) const {
        switch (index) {
            case 0: return ability1();
            case 1: return ability2();
            case 2: return abilityH();
            default: return 0;
        }
    }

    int abilityCount() const { return 3; }

    // Magic gender ratio constants from PKHeX
    static constexpr uint8_t RatioMagicGenderless = 255;
    static constexpr uint8_t RatioMagicFemale = 254;
    static constexpr uint8_t RatioMagicMale = 0;
};

class PersonalTable {
public:
    static constexpr int ENTRY_SIZE = 0x50;

    bool load(const std::string& path);

    PersonalInfo9SV operator[](int index) const {
        if (index < 0 || index >= count_)
            index = 0;
        return { raw_.data() + index * ENTRY_SIZE };
    }

    PersonalInfo9SV getFormEntry(uint16_t species, uint8_t form) const {
        int idx = getFormIndex(species, form);
        return (*this)[idx];
    }

    int count() const { return count_; }

private:
    std::vector<uint8_t> raw_;
    int count_ = 0;

    int getFormIndex(uint16_t species, uint8_t form) const {
        if (species >= count_) return 0;
        auto entry = (*this)[(int)species];
        if (form == 0) return species;
        if (form >= entry.formCount()) return species;
        int fsi = entry.formStatsIndex();
        if (fsi == 0) return species;
        int idx = fsi + form - 1;
        if (idx >= count_) return species;
        return idx;
    }
};

#pragma once
#include "sc_block.h"
#include "swish_crypto.h"
#include <cstdint>
#include <vector>
#include <cstring>

// Enums matching PKHeX/TeraFinder
enum class TeraRaidContentType : uint32_t {
    Base05 = 0,
    Black6 = 1,
    Distribution = 2,
    Might7 = 3,
};

enum class TeraRaidMapParent : uint8_t {
    Paldea = 0,
    Kitakami = 1,
    Blueberry = 2,
};

enum class GameProgress : uint8_t {
    Beginning = 0,
    UnlockedTeraRaids = 1,
    Unlocked3Stars = 2,
    Unlocked4Stars = 3,
    Unlocked5Stars = 4,
    Unlocked6Stars = 5,
};

enum class RaidContent : uint8_t {
    Standard = 0,
    Black = 1,
    Event = 2,
    Event_Mighty = 3,
};

// Tera Raid Detail - 0x20 bytes per entry in save
// From PKHeX.Core/Saves/Substructures/Gen9/SV/RaidSpawnList9.cs
struct TeraRaidDetail {
    static constexpr int SIZE = 0x20;

    bool isEnabled;
    uint32_t areaID;
    uint32_t lotteryGroup;
    uint32_t spawnPointID;
    uint32_t seed;
    TeraRaidContentType content;
    bool isClaimedLP;

    static TeraRaidDetail readFrom(const uint8_t* data) {
        TeraRaidDetail d;
        auto r32 = [](const uint8_t* p) -> uint32_t {
            return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
        };
        d.isEnabled      = r32(data + 0x00) != 0;
        d.areaID         = r32(data + 0x04);
        d.lotteryGroup   = r32(data + 0x08);
        d.spawnPointID   = r32(data + 0x0C);
        d.seed           = r32(data + 0x10);
        d.content        = (TeraRaidContentType)r32(data + 0x18);
        d.isClaimedLP    = r32(data + 0x1C) != 0;
        return d;
    }

    RaidContent raidContent() const {
        switch (content) {
            case TeraRaidContentType::Base05:        return RaidContent::Standard;
            case TeraRaidContentType::Black6:        return RaidContent::Black;
            case TeraRaidContentType::Distribution:  return RaidContent::Event;
            case TeraRaidContentType::Might7:        return RaidContent::Event_Mighty;
        }
        return RaidContent::Standard;
    }
};

// SCBlock keys for raid data
namespace RaidBlockKeys {
    constexpr uint32_t KTeraRaidPaldea          = 0xCAAC8800;
    constexpr uint32_t KTeraRaidDLC             = 0x100B93DA;
    constexpr uint32_t KMyStatus                = 0xE3E89BD1;
    // Unlock flags (encrypted bool blocks: Bool2 = true, Bool1 = false)
    constexpr uint32_t KUnlockedTeraRaidBattles = 0x27025EBF;
    constexpr uint32_t KUnlockedRaidDifficulty3 = 0xEC95D8EF;
    constexpr uint32_t KUnlockedRaidDifficulty4 = 0xA9428DFE;
    constexpr uint32_t KUnlockedRaidDifficulty5 = 0x9535F471;
    constexpr uint32_t KUnlockedRaidDifficulty6 = 0x6E7F8220;
}

// Parse raid entries from raw bytes or SCBlocks
struct RaidBlockData {
    std::vector<TeraRaidDetail> paldea;
    std::vector<TeraRaidDetail> kitakami;
    std::vector<TeraRaidDetail> blueberry;

    // Parse Paldea block (has 0x10 seed header + up to 72 raids)
    static void parsePaldea(const uint8_t* data, size_t len, std::vector<TeraRaidDetail>& out) {
        if (len < 0x10) return;
        const uint8_t* base = data + 0x10; // skip seed header
        size_t dataLen = len - 0x10;
        int count = (int)(dataLen / TeraRaidDetail::SIZE);
        if (count > 72) count = 72;
        for (int i = 0; i < count; i++)
            out.push_back(TeraRaidDetail::readFrom(base + i * TeraRaidDetail::SIZE));
    }

    // Parse DLC block: Kitakami [0, 0xC80) + Blueberry [0xC80, 0x1900)
    static void parseDLC(const uint8_t* data, size_t len,
                         std::vector<TeraRaidDetail>& kitakami,
                         std::vector<TeraRaidDetail>& blueberry) {
        constexpr int regionSize = 0xC80; // 100 * 0x20
        if (len >= (size_t)regionSize) {
            int kitaCount = regionSize / TeraRaidDetail::SIZE;
            if (kitaCount > 100) kitaCount = 100;
            for (int i = 0; i < kitaCount; i++)
                kitakami.push_back(TeraRaidDetail::readFrom(data + i * TeraRaidDetail::SIZE));
        }
        if (len >= 0x1900) {
            const uint8_t* blueBase = data + regionSize;
            int blueCount = regionSize / TeraRaidDetail::SIZE;
            if (blueCount > 80) blueCount = 80;
            for (int i = 0; i < blueCount; i++)
                blueberry.push_back(TeraRaidDetail::readFrom(blueBase + i * TeraRaidDetail::SIZE));
        }
    }

    // Extract from decrypted SCBlocks (save file path)
    static RaidBlockData extract(const std::vector<SCBlock>& blocks) {
        RaidBlockData result;

        const SCBlock* paldeaBlock = SwishCrypto::findBlock(blocks, RaidBlockKeys::KTeraRaidPaldea);
        if (paldeaBlock && !paldeaBlock->data.empty())
            parsePaldea(paldeaBlock->data.data(), paldeaBlock->data.size(), result.paldea);

        const SCBlock* dlcBlock = SwishCrypto::findBlock(blocks, RaidBlockKeys::KTeraRaidDLC);
        if (dlcBlock && !dlcBlock->data.empty())
            parseDLC(dlcBlock->data.data(), dlcBlock->data.size(), result.kitakami, result.blueberry);

        return result;
    }
};

// Extract GameProgress from unlock flag blocks
inline GameProgress getGameProgress(const std::vector<SCBlock>& blocks) {
    auto checkBool = [&](uint32_t key) -> bool {
        const SCBlock* b = SwishCrypto::findBlock(blocks, key);
        return b && b->type == SCTypeCode::Bool2;
    };

    if (checkBool(RaidBlockKeys::KUnlockedRaidDifficulty6))
        return GameProgress::Unlocked6Stars;
    if (checkBool(RaidBlockKeys::KUnlockedRaidDifficulty5))
        return GameProgress::Unlocked5Stars;
    if (checkBool(RaidBlockKeys::KUnlockedRaidDifficulty4))
        return GameProgress::Unlocked4Stars;
    if (checkBool(RaidBlockKeys::KUnlockedRaidDifficulty3))
        return GameProgress::Unlocked3Stars;
    if (checkBool(RaidBlockKeys::KUnlockedTeraRaidBattles))
        return GameProgress::UnlockedTeraRaids;

    return GameProgress::Beginning;
}

// Extract trainer ID32 from MyStatus block
inline uint32_t getTrainerID32(const std::vector<SCBlock>& blocks) {
    const SCBlock* b = SwishCrypto::findBlock(blocks, RaidBlockKeys::KMyStatus);
    if (b && b->data.size() >= 8) {
        // ID32 is at offset 0x04 in MyStatus (u16 TID + u16 SID packed as u32)
        const uint8_t* d = b->data.data() + 0x04;
        return d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
    }
    return 0;
}

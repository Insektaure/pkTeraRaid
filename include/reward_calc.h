#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

struct RewardItem {
    uint16_t itemId;
    uint8_t  amount;
    int8_t   subjectType;  // 0=host, 1=joiner, 2=everyone
};

class RewardCalc {
public:
    bool loadTables(const std::string& fixedPath, const std::string& lotteryPath);

    std::vector<RewardItem> calculateRewards(uint32_t seed, uint8_t stars,
        uint64_t fixedHash, uint64_t lotteryHash,
        uint16_t species, uint8_t teraType) const;

    static uint16_t getTeraShardId(uint8_t teraType);
    static uint16_t getMaterialId(uint16_t species);

private:
    struct FixedEntry {
        uint8_t  category;
        uint16_t itemId;
        uint8_t  amount;
        int8_t   subjectType;
    };

    struct LotteryEntry {
        uint8_t  category;
        uint16_t itemId;
        uint8_t  amount;
        uint16_t rate;
    };

    struct LotteryTable {
        uint16_t totalRate;
        std::vector<LotteryEntry> items;
    };

    std::unordered_map<uint64_t, std::vector<FixedEntry>> fixedTables_;
    std::unordered_map<uint64_t, LotteryTable> lotteryTables_;

    static int getRewardCount(uint64_t random, int stars);
};

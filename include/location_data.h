#pragma once
#include "tera_raid.h"
#include <string>
#include <unordered_map>

struct RaidCoord {
    float x, y, z;
};

// Loads raid den locations from JSON files
// Format: { "AreaID-LotteryGroup-SpawnPointID": [x, y, z], ... }
class LocationData {
public:
    bool load(const std::string& paldeaPath,
              const std::string& kitakamiPath,
              const std::string& blueberryPath);

    // Get coordinates for a raid. Returns false if not found.
    bool getCoord(TeraRaidMapParent map, uint32_t areaID, uint32_t lotteryGroup, uint32_t spawnPointID, RaidCoord& out) const;

    // Get min/max bounds for a map (for rendering)
    void getBounds(TeraRaidMapParent map, float& minX, float& maxX, float& minZ, float& maxZ) const;

private:
    std::unordered_map<std::string, RaidCoord> paldea_;
    std::unordered_map<std::string, RaidCoord> kitakami_;
    std::unordered_map<std::string, RaidCoord> blueberry_;

    static bool loadJson(const std::string& path, std::unordered_map<std::string, RaidCoord>& out);
    static std::string makeKey(uint32_t area, uint32_t lottery, uint32_t spawn);

    const std::unordered_map<std::string, RaidCoord>& getMap(TeraRaidMapParent m) const {
        switch (m) {
            case TeraRaidMapParent::Kitakami:  return kitakami_;
            case TeraRaidMapParent::Blueberry: return blueberry_;
            default:                           return paldea_;
        }
    }
};

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

    struct MapBounds { float minX, maxX, minZ, maxZ; };
    MapBounds paldeaBounds_{};
    MapBounds kitakamiBounds_{};
    MapBounds blueberryBounds_{};

    static bool loadJson(const std::string& path, std::unordered_map<std::string, RaidCoord>& out);
    static std::string makeKey(uint32_t area, uint32_t lottery, uint32_t spawn);
    static MapBounds computeBounds(const std::unordered_map<std::string, RaidCoord>& m);

    const std::unordered_map<std::string, RaidCoord>& getMap(TeraRaidMapParent m) const {
        switch (m) {
            case TeraRaidMapParent::Kitakami:  return kitakami_;
            case TeraRaidMapParent::Blueberry: return blueberry_;
            default:                           return paldea_;
        }
    }

    const MapBounds& getBoundsCache(TeraRaidMapParent m) const {
        switch (m) {
            case TeraRaidMapParent::Kitakami:  return kitakamiBounds_;
            case TeraRaidMapParent::Blueberry: return blueberryBounds_;
            default:                           return paldeaBounds_;
        }
    }
};

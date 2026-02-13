#include "location_data.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <vector>

std::string LocationData::makeKey(uint32_t area, uint32_t lottery, uint32_t spawn) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u-%u-%u", area, lottery, spawn);
    return buf;
}

// Minimal JSON parser for our specific format: { "key": [x, y, z], ... }
bool LocationData::loadJson(const std::string& path, std::unordered_map<std::string, RaidCoord>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<char> buf(size + 1);
    fread(buf.data(), 1, size, f);
    fclose(f);
    buf[size] = 0;

    const char* p = buf.data();
    while (*p && *p != '{') p++;
    if (!*p) return false;
    p++;

    while (*p) {
        // Find opening quote for key
        while (*p && *p != '"' && *p != '}') p++;
        if (!*p || *p == '}') break;
        p++; // skip opening "

        // Read key
        const char* keyStart = p;
        while (*p && *p != '"') p++;
        std::string key(keyStart, p - keyStart);
        if (*p) p++; // skip closing "

        // Find opening [
        while (*p && *p != '[') p++;
        if (!*p) break;
        p++;

        // Parse 3 floats
        RaidCoord coord{};
        char* end;
        coord.x = strtof(p, &end); p = end;
        while (*p == ',' || *p == ' ') p++;
        coord.y = strtof(p, &end); p = end;
        while (*p == ',' || *p == ' ') p++;
        coord.z = strtof(p, &end); p = end;

        // Find closing ]
        while (*p && *p != ']') p++;
        if (*p) p++;

        out[key] = coord;
    }

    return !out.empty();
}

bool LocationData::load(const std::string& paldeaPath,
                        const std::string& kitakamiPath,
                        const std::string& blueberryPath) {
    bool ok = loadJson(paldeaPath, paldea_);
    loadJson(kitakamiPath, kitakami_);
    loadJson(blueberryPath, blueberry_);
    return ok;
}

bool LocationData::getCoord(TeraRaidMapParent map, uint32_t areaID, uint32_t lotteryGroup, uint32_t spawnPointID, RaidCoord& out) const {
    auto& m = getMap(map);
    auto key = makeKey(areaID, lotteryGroup, spawnPointID);
    auto it = m.find(key);
    if (it == m.end()) return false;
    out = it->second;
    return true;
}

void LocationData::getBounds(TeraRaidMapParent map, float& minX, float& maxX, float& minZ, float& maxZ) const {
    auto& m = getMap(map);
    minX = FLT_MAX; maxX = -FLT_MAX;
    minZ = FLT_MAX; maxZ = -FLT_MAX;
    for (auto& [key, coord] : m) {
        if (coord.x < minX) minX = coord.x;
        if (coord.x > maxX) maxX = coord.x;
        if (coord.z < minZ) minZ = coord.z;
        if (coord.z > maxZ) maxZ = coord.z;
    }
    // Add some padding
    float padX = (maxX - minX) * 0.05f;
    float padZ = (maxZ - minZ) * 0.05f;
    minX -= padX; maxX += padX;
    minZ -= padZ; maxZ += padZ;
}

#pragma once
#include <cstdint>

enum class PlaRegion : uint8_t {
    ObsidianFieldlands = 0,
    CrimsonMirelands   = 1,
    CobaltCoastlands   = 2,
    CoronetHighlands   = 3,
    AlabasterIcelands  = 4,
    Count              = 5,
};

inline const char* plaRegionName(PlaRegion r) {
    switch (r) {
        case PlaRegion::ObsidianFieldlands: return "Obsidian Fieldlands";
        case PlaRegion::CrimsonMirelands:   return "Crimson Mirelands";
        case PlaRegion::CobaltCoastlands:   return "Cobalt Coastlands";
        case PlaRegion::CoronetHighlands:   return "Coronet Highlands";
        case PlaRegion::AlabasterIcelands:  return "Alabaster Icelands";
        default: return "?";
    }
}

inline const char* plaRegionMapFile(PlaRegion r) {
    switch (r) {
        case PlaRegion::ObsidianFieldlands: return "obsidian_fieldlands.jpg";
        case PlaRegion::CrimsonMirelands:   return "crimson_mirelands.jpg";
        case PlaRegion::CobaltCoastlands:   return "cobalt_coastlands.jpg";
        case PlaRegion::CoronetHighlands:   return "coronet_highlands.jpg";
        case PlaRegion::AlabasterIcelands:  return "alabaster_icelands.jpg";
        default: return "";
    }
}

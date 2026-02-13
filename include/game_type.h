#pragma once
#include <cstdint>

enum class GameVersion { Scarlet, Violet };

constexpr uint64_t SCARLET_TITLE_ID = 0x0100A3D008C5C000ULL;
constexpr uint64_t VIOLET_TITLE_ID  = 0x01008F6008C5E000ULL;

inline uint64_t titleIdOf(GameVersion g) {
    switch (g) {
        case GameVersion::Scarlet: return SCARLET_TITLE_ID;
        case GameVersion::Violet:  return VIOLET_TITLE_ID;
    }
    return 0;
}

inline const char* gameDisplayNameOf(GameVersion g) {
    switch (g) {
        case GameVersion::Scarlet: return "Pokemon Scarlet";
        case GameVersion::Violet:  return "Pokemon Violet";
    }
    return "";
}

inline const char* saveFileNameOf(GameVersion) {
    return "main";
}

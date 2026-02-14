#pragma once
#include <cstdint>

enum class GameVersion { Scarlet, Violet, Sword, Shield };

constexpr uint64_t SCARLET_TITLE_ID = 0x0100A3D008C5C000ULL;
constexpr uint64_t VIOLET_TITLE_ID  = 0x01008F6008C5E000ULL;
constexpr uint64_t SWORD_TITLE_ID   = 0x0100ABF008968000ULL;
constexpr uint64_t SHIELD_TITLE_ID  = 0x01008DB008C2C000ULL;

inline uint64_t titleIdOf(GameVersion g) {
    switch (g) {
        case GameVersion::Scarlet: return SCARLET_TITLE_ID;
        case GameVersion::Violet:  return VIOLET_TITLE_ID;
        case GameVersion::Sword:   return SWORD_TITLE_ID;
        case GameVersion::Shield:  return SHIELD_TITLE_ID;
    }
    return 0;
}

inline const char* gameDisplayNameOf(GameVersion g) {
    switch (g) {
        case GameVersion::Scarlet: return "Pokemon Scarlet";
        case GameVersion::Violet:  return "Pokemon Violet";
        case GameVersion::Sword:   return "Pokemon Sword";
        case GameVersion::Shield:  return "Pokemon Shield";
    }
    return "";
}

inline bool isSwSh(GameVersion g) {
    return g == GameVersion::Sword || g == GameVersion::Shield;
}

inline bool isSV(GameVersion g) {
    return g == GameVersion::Scarlet || g == GameVersion::Violet;
}

inline const char* saveFileNameOf(GameVersion) {
    return "main";
}

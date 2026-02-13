#pragma once

#ifdef __SWITCH__
#include <switch.h>
#include <switch/dmntcht.h>
#include <cstdint>
#include <cstdio>

// Pointer chains from Tera-Finder DataBlocks.cs (SV 3.0.1)
namespace DmntPointers {
    constexpr uint64_t KTeraRaidPaldea[] = {0x47350D8, 0x1C0, 0x88, 0x40};
    constexpr int      KTeraRaidPaldeaLen = 4;
    constexpr size_t   KTeraRaidPaldeaSize = 0xC98;

    constexpr uint64_t KTeraRaidDLC[] = {0x47350D8, 0x1C0, 0x88, 0xCD8};
    constexpr int      KTeraRaidDLCLen = 4;
    constexpr size_t   KTeraRaidDLCSize = 0x1910;

    constexpr uint64_t KMyStatus[] = {0x47350D8, 0xD8, 0x08, 0xB8, 0x0, 0x40};
    constexpr int      KMyStatusLen = 6;
    constexpr size_t   KMyStatusSize = 0x68;
}

namespace DmntMem {

inline DmntCheatProcessMetadata g_meta = {};
inline bool g_initialized = false;

inline bool init() {
    Result rc = dmntchtInitialize();
    if (R_FAILED(rc)) return false;

    bool hasProc = false;
    rc = dmntchtHasCheatProcess(&hasProc);
    if (R_FAILED(rc) || !hasProc) {
        // Try force-opening
        rc = dmntchtForceOpenCheatProcess();
        if (R_FAILED(rc)) {
            dmntchtExit();
            return false;
        }
    }

    rc = dmntchtGetCheatProcessMetadata(&g_meta);
    if (R_FAILED(rc)) {
        dmntchtExit();
        return false;
    }

    g_initialized = true;
    return true;
}

inline void exit() {
    if (g_initialized) {
        dmntchtExit();
        g_initialized = false;
    }
}

inline uint64_t titleId() {
    return g_meta.title_id;
}

// Resolve a pointer chain and read a block of memory.
// chain[0] is the offset from main NSO base.
// chain[1..n-1] are dereference offsets.
inline bool readBlock(const uint64_t* chain, int chainLen, uint8_t* buf, size_t size) {
    if (!g_initialized || chainLen < 1) return false;

    uint64_t addr = g_meta.main_nso_extents.base + chain[0];

    for (int i = 1; i < chainLen; i++) {
        uint64_t ptr = 0;
        Result rc = dmntchtReadCheatProcessMemory(addr, &ptr, sizeof(uint64_t));
        if (R_FAILED(rc) || ptr == 0) return false;
        addr = ptr + chain[i];
    }

    Result rc = dmntchtReadCheatProcessMemory(addr, buf, size);
    return R_SUCCEEDED(rc);
}

} // namespace DmntMem

#endif // __SWITCH__

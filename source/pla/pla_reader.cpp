#include "pla/pla_reader.h"
#include "pla/pla_markers.h"
#include "dmnt_mem.h"
#include "xoroshiro128plus.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {

bool readAt(const uint64_t* base, int baseLen, uint64_t tail, uint8_t* buf, size_t size) {
    uint64_t chain[8];
    if (baseLen + 1 > (int)(sizeof(chain) / sizeof(chain[0]))) return false;
    for (int i = 0; i < baseLen; i++) chain[i] = base[i];
    chain[baseLen] = tail;
    return DmntMem::readBlock(chain, baseLen + 1, buf, size);
}

bool readU64(const uint64_t* base, int baseLen, uint64_t tail, uint64_t& out) {
    uint8_t buf[8];
    if (!readAt(base, baseLen, tail, buf, 8)) return false;
    std::memcpy(&out, buf, 8);
    return true;
}

bool readU32(const uint64_t* base, int baseLen, uint64_t tail, uint32_t& out) {
    uint8_t buf[4];
    if (!readAt(base, baseLen, tail, buf, 4)) return false;
    std::memcpy(&out, buf, 4);
    return true;
}

bool readFloat3(const uint64_t* base, int baseLen, uint64_t tail, float out[3]) {
    uint8_t buf[12];
    if (!readAt(base, baseLen, tail, buf, 12)) return false;
    std::memcpy(out, buf, 12);
    return true;
}

#ifdef PLA_OVERLAY_POLL
// Resolve the spawner array base once: [[main+42A6EE0]+330]. 2 IPC reads.
bool resolveSpawnerBase(uint64_t& out) {
    const uint64_t chain[3] = {PlaPointers::SPAWNER_BASE[0], PlaPointers::SPAWNER_BASE[1], 0};
    return DmntMem::resolveChain(chain, 3, out);
}

#endif // PLA_OVERLAY_POLL

} // anonymous

#ifdef PLA_OVERLAY_POLL

bool PlaReader::pollLiveCount(uint32_t& outCount) {
    uint64_t base = 0;
    if (!resolveSpawnerBase(base)) return false;

    uint32_t raw = 0;
    if (!DmntMem::readAbsolute(base + PlaPointers::SIZE_FIELD,
                               reinterpret_cast<uint8_t*>(&raw), sizeof(raw)))
        return false;

    outCount = raw / 0x40;
    return true;
}

bool PlaReader::pollGroupSeeds(uint64_t* outSeeds, int maxGroups) {
    using namespace PlaPointers;
    if (!outSeeds || maxGroups <= 0) return false;

    uint64_t base = 0;
    if (!resolveSpawnerBase(base)) return false;

    for (int gid = 0; gid < maxGroups; gid++) outSeeds[gid] = 0;

    // Group gid's generator seed sits at POS_POSITION + gid*GROUP_STRIDE +
    // GROUP_GEN_SEED. Those are 8 useful bytes every 0x440, so read the window
    // in big chunks and pick the seeds out locally - far fewer round trips
    // than one read per group.
    constexpr size_t CHUNK = 0x8000;  // 32 KiB
    static uint8_t buf[CHUNK];

    const uint64_t windowStart = POS_POSITION;
    const uint64_t windowEnd   = POS_POSITION
                               + (uint64_t)(maxGroups - 1) * GROUP_STRIDE
                               + GROUP_GEN_SEED + sizeof(uint64_t);

    for (uint64_t start = windowStart; start < windowEnd; start += CHUNK) {
        const size_t len = (size_t)std::min<uint64_t>(CHUNK, windowEnd - start);
        if (!DmntMem::readAbsolute(base + start, buf, len)) return false;

        for (int gid = 0; gid < maxGroups; gid++) {
            const uint64_t off = POS_POSITION + (uint64_t)gid * GROUP_STRIDE + GROUP_GEN_SEED;
            if (off < start || off + sizeof(uint64_t) > start + len) continue;
            std::memcpy(&outSeeds[gid], buf + (off - start), sizeof(uint64_t));
        }
    }
    return true;
}

void PlaReader::setGroupSeeds(const uint64_t* seeds, int count) {
    // livePositions_ is deliberately left alone: it is only used for the
    // `active` flag, and refreshing it is what makes a full readLive() costly.
    spawners_.clear();

    for (int gid = 0; gid < count; gid++) {
        if (seeds[gid] == 0) continue;

        PlaSpawner s{};
        s.groupId = gid;
        s.generatorSeed = seeds[gid];
        s.groupSeed = seeds[gid] - Xoroshiro128Plus::XOROSHIRO_CONST;
        s.region = -1;
        spawners_.push_back(s);
    }
}

#endif // PLA_OVERLAY_POLL

bool PlaReader::readLive(int maxGroups) {
    spawners_.clear();
    livePositions_.clear();

    using namespace PlaPointers;

    // Runtime live-position list (flat index into currently-rendered spawns,
    // NOT indexed by group_id - must match by world coords later).
    uint32_t rawSize = 0;
    if (readU32(SPAWNER_BASE, SPAWNER_BASE_LEN, SIZE_FIELD, rawSize)) {
        int posCount = (int)(rawSize / 0x40);
        if (posCount > 0 && posCount < 4096) {
            for (int i = 0; i < posCount; i++) {
                float pos[3];
                uint64_t seed = 0;
                if (!readFloat3(SPAWNER_BASE, SPAWNER_BASE_LEN,
                                POS_POSITION + i * POS_STRIDE, pos)) continue;
                if (!readU64(SPAWNER_BASE, SPAWNER_BASE_LEN,
                             POS_SEED + i * POS_STRIDE, seed)) continue;
                if (seed == 0) continue;
                if (pos[0] < 1.0f && pos[1] < 1.0f && pos[2] < 1.0f) continue;
                livePositions_.push_back({pos[0], pos[1], pos[2]});
            }
        }
    }

    // Group data (0x440 stride). Read raw seeds only - decoration is per-region.
    for (int gid = 0; gid < maxGroups; gid++) {
        uint64_t generatorSeed = 0;
        if (!readU64(SPAWNER_BASE, SPAWNER_BASE_LEN,
                     POS_POSITION + gid * GROUP_STRIDE + GROUP_GEN_SEED, generatorSeed))
            break;
        if (generatorSeed == 0) continue;

        PlaSpawner s{};
        s.groupId = gid;
        s.generatorSeed = generatorSeed;
        s.groupSeed = generatorSeed - Xoroshiro128Plus::XOROSHIRO_CONST;
        s.region = -1;
        spawners_.push_back(s);
    }
    return true;
}

void PlaReader::decorate(PlaRegion region, int shinyRolls, int maxAdvance) {
    size_t n;
    const PlaMarkers::Marker* ms = PlaMarkers::getMarkers(region, n);

    for (auto& s : spawners_) {
        const PlaMarkers::Marker* m = nullptr;
        for (size_t i = 0; i < n; i++) {
            if (ms[i].groupId == s.groupId) { m = &ms[i]; break; }
        }

        if (!m) {
            s.region = -1;
            s.markerX = s.markerY = s.markerZ = 0.0f;
            s.guaranteedIvs = 0;
            s.active = false;
            s.livePosX = s.livePosY = s.livePosZ = 0.0f;
            s.speciesName = nullptr;
            s.alpha = false;
            s.firstSpawn = PlaEncounter::firstFixed(s.groupSeed, shinyRolls, 0);
            s.shinyAdvance = PlaEncounter::nextShiny(s.groupSeed, shinyRolls,
                                                     0, true, maxAdvance);
            continue;
        }

        s.region = (int)region;
        s.markerX = m->x; s.markerY = m->y; s.markerZ = m->z;
        s.guaranteedIvs = m->ivs;

        // Active = any runtime live position within ~25 world units of this marker.
        s.active = false;
        s.livePosX = s.livePosY = s.livePosZ = 0.0f;
        constexpr float R2 = 25.0f * 25.0f;
        for (auto& lp : livePositions_) {
            float dx = lp.x - m->x;
            float dz = lp.z - m->z;
            if (dx*dx + dz*dz <= R2) {
                s.active = true;
                s.livePosX = lp.x; s.livePosY = lp.y; s.livePosZ = lp.z;
                break;
            }
        }

        double slotRoll01 = 0.0;
        s.firstSpawn   = PlaEncounter::firstFixed(s.groupSeed, shinyRolls,
                                                  s.guaranteedIvs, &slotRoll01);
        s.shinyAdvance = PlaEncounter::nextShiny(s.groupSeed, shinyRolls,
                                                 s.guaranteedIvs, true, maxAdvance);

        s.speciesName = nullptr;
        s.speciesId = 0;
        s.form = 0;
        s.alpha = false;
        if (m->slotCount > 0 && m->slotTotal > 0) {
            const auto* entry = PlaMarkers::resolveSlot(*m, slotRoll01 * m->slotTotal);
            if (entry) {
                s.speciesName = entry->name;
                s.speciesId   = entry->speciesId;
                s.form        = entry->form;
                s.alpha       = entry->alpha;
            }
        }
    }
}

bool PlaReader::teleport(float x, float y, float z) {
    float pos[3] = {x, y, z};
    return DmntMem::writeBlock(PlaPointers::PLAYER_POS_CHAIN,
                               PlaPointers::PLAYER_POS_CHAIN_LEN,
                               reinterpret_cast<const uint8_t*>(pos),
                               sizeof(pos));
}

PlaOutbreak PlaReader::readOutbreak() {
    PlaOutbreak ob{};
    ob.present = false;

    using namespace PlaPointers;
    for (int i = 0; i < 4; i++) {
        uint8_t raw[1];
        if (!readAt(OUTBREAK_BASE, OUTBREAK_BASE_LEN, 0x60 + i * 0x50, raw, 1)) continue;
        if (raw[0] >= 10 && raw[0] <= 15) {
            ob.spawnCount = raw[0];
            ob.present = true;
            break;
        }
    }
    if (!ob.present) return ob;

    for (int gid = 255; gid >= 0; gid--) {
        uint64_t activeSeed = 0;
        if (!readU64(SPAWNER_BASE, SPAWNER_BASE_LEN,
                     POS_POSITION + gid * GROUP_STRIDE + GROUP_ACTIVE_SEED, activeSeed))
            break;
        if (activeSeed != 0) {
            uint64_t genSeed = 0;
            readU64(SPAWNER_BASE, SPAWNER_BASE_LEN,
                    POS_POSITION + gid * GROUP_STRIDE + GROUP_GEN_SEED, genSeed);
            ob.groupId = gid;
            ob.groupSeed = genSeed - Xoroshiro128Plus::XOROSHIRO_CONST;
            return ob;
        }
    }
    ob.present = false;
    return ob;
}

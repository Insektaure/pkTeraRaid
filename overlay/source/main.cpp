// pkTeraRaid — libultrahand overlay: quick-glance PLA shiny summary.
// Root menu lists the 5 Hisui regions; tapping one opens a detail view
// with current shinies + near-advance spawners. Y rescans memory.
// L+R detaches: the overlay stays on screen but the game keeps input.

#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "detach_gui.h"

#include "pla/pla_reader.h"
#include "pla/pla_markers.h"
#include "pla/pla_region.h"
#include "dmnt_mem.h"
#include "game_type.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int SCAN_MAX_GROUPS = 256;
constexpr int SHINY_SEARCH    = 5000;
constexpr int NEAR_THRESHOLD  = 100;
constexpr size_t TOP_N        = 10;

// Cache backing the detached HUD's change detection.
struct PollCache {
    uint32_t liveCount = 0;
    bool     haveLiveCount = false;
    int      ticksSinceFullPoll = 0;
    uint64_t seeds[SCAN_MAX_GROUPS] = {};
    bool     haveSeeds = false;
};

struct OverlayState {
    PlaReader reader;
    bool      scanned = false;
    std::string status;
    PollCache poll;
};
inline OverlayState& state() {
    static OverlayState s;
    return s;
}

void performScan() {
    auto& s = state();
    s.status.clear();
    if (!DmntMem::init()) {
        s.status = "dmnt:cht: could not attach";
        return;
    }
    uint64_t tid = DmntMem::titleId();
    if (tid != PLA_TITLE_ID) {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "Wrong title: %016lX (need PLA)", (unsigned long)tid);
        s.status = buf;
        return;
    }
    if (!s.reader.readLive(SCAN_MAX_GROUPS)) {
        s.status = "readLive() failed";
        return;
    }
    s.scanned = true;
}

void ensureScanned() {
    if (!state().scanned) performScan();
}

// Full group-seed comparison runs at most every Nth poll even when the cheap
// signal is quiet, so an in-place re-roll can't hide forever.
constexpr int FULL_POLL_EVERY = 10;

// Detached-HUD change detection, cheapest stage first. Returns true only when
// the spawner data really moved, which is the only case worth re-rendering.
bool pollForSpawnerChanges() {
    auto& s = state();
    if (!s.scanned) return false;  // nothing good to compare against

    // Stage 1 (~3 IPC reads): has anything spawned, despawned or been caught?
    uint32_t liveCount = 0;
    const bool gotCount = s.reader.pollLiveCount(liveCount);
    const bool countMoved = !gotCount || !s.poll.haveLiveCount || liveCount != s.poll.liveCount;
    s.poll.haveLiveCount = gotCount;
    s.poll.liveCount = liveCount;

    const bool dueFullPoll = ++s.poll.ticksSinceFullPoll >= FULL_POLL_EVERY;
    if (!countMoved && !dueFullPoll) return false;
    s.poll.ticksSinceFullPoll = 0;

    // Stage 2 (~11 IPC reads): did any group actually re-roll?
    uint64_t seeds[SCAN_MAX_GROUPS];
    if (!s.reader.pollGroupSeeds(seeds, SCAN_MAX_GROUPS)) return false;
    if (s.poll.haveSeeds && std::memcmp(seeds, s.poll.seeds, sizeof(seeds)) == 0) return false;

    // Stage 3 (CPU only): rebuild the spawner list and re-derive the rows.
    std::memcpy(s.poll.seeds, seeds, sizeof(seeds));
    s.poll.haveSeeds = true;
    s.reader.setGroupSeeds(seeds, SCAN_MAX_GROUPS);
    return true;
}

struct RegionStats { size_t current; size_t near; size_t total; };

// Compute stats for `region` after decorating. Note: decorate() overwrites the
// cached per-spawner fields, so we re-decorate each time we need fresh numbers
// (cheap — no memory I/O, just RNG).
RegionStats statsFor(PlaRegion region) {
    auto& s = state();
    s.reader.decorate(region, 1, SHINY_SEARCH);
    RegionStats out{0, 0, 0};
    const auto& sps = s.reader.spawners();
    for (const auto& sp : sps) {
        if (sp.region != (int)region) continue;
        if (sp.speciesId == 0) continue;  // skip story/placeholder spawners
        out.total++;
        if (sp.shinyAdvance == 0 && sp.firstSpawn.shiny) out.current++;
        else if (sp.shinyAdvance > 0 && sp.shinyAdvance <= NEAR_THRESHOLD) out.near++;
    }
    return out;
}

// One display row: the formatted label plus the spawner it came from, so the
// attached list can wire up teleport and the detached HUD can reuse the text.
struct RegionRow {
    std::string text;
    float x, y, z;
};

struct RegionView {
    std::string summary;
    std::vector<RegionRow> current;  // shiny on the current advance
    std::vector<RegionRow> near;     // shiny within NEAR_THRESHOLD advances
};

// Decorate `region` and format its rows. Shared by both render modes.
RegionView buildRegionView(PlaRegion region) {
    auto& s = state();
    s.reader.decorate(region, 1, SHINY_SEARCH);

    const auto& sps = s.reader.spawners();
    std::vector<int> current, nearN;
    size_t farCount = 0;
    for (int i = 0; i < (int)sps.size(); i++) {
        const auto& sp = sps[i];
        if (sp.region != (int)region) continue;
        if (sp.speciesId == 0) continue;  // skip story/placeholder spawners
        if (sp.shinyAdvance == 0 && sp.firstSpawn.shiny) current.push_back(i);
        else if (sp.shinyAdvance > 0 && sp.shinyAdvance <= NEAR_THRESHOLD) nearN.push_back(i);
        else if (sp.shinyAdvance > NEAR_THRESHOLD) farCount++;
    }
    std::sort(nearN.begin(), nearN.end(),
              [&](int a, int b){ return sps[a].shinyAdvance < sps[b].shinyAdvance; });

    RegionView view;
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "Current: %zu    <%d adv: %zu    total: %zu",
                  current.size(), NEAR_THRESHOLD, nearN.size(),
                  current.size() + nearN.size() + farCount);
    view.summary = buf;

    auto makeRow = [&](int idx) {
        const auto& sp = sps[idx];
        char row[96];
        const char* species = sp.speciesName ? sp.speciesName : "?";
        const char* mark    = sp.alpha ? "a " : "  ";
        if (sp.shinyAdvance == 0)
            std::snprintf(row, sizeof(row), "%s%s  NOW  (g%d)",
                          mark, species, sp.groupId);
        else
            std::snprintf(row, sizeof(row), "%s%s  in %d  (g%d)",
                          mark, species, sp.shinyAdvance, sp.groupId);
        // Raw marker coords — matches PLA-Live-Map reference impl.
        return RegionRow{row, sp.markerX, sp.markerY, sp.markerZ};
    };

    for (int idx : current) view.current.push_back(makeRow(idx));
    for (int idx : nearN) {
        if (view.near.size() >= TOP_N) break;
        view.near.push_back(makeRow(idx));
    }
    return view;
}

// Forward decls
class RegionDetailsGui;
class RegionListGui;

class RegionDetailsGui : public DetachableGui {
public:
    explicit RegionDetailsGui(PlaRegion r) : region_(r) {}

protected:
    std::string title() override { return plaRegionName(region_); }

    std::string subtitle() override {
        return detach::attached() ? "pkTeraRaid  " + detach::hint() : detach::hint();
    }

    void buildList(tsl::elm::List* list) override {
        auto& s = state();
        if (!s.status.empty()) {
            list->addItem(new tsl::elm::ListItem(s.status));
            return;
        }

        const RegionView view = buildRegionView(region_);
        list->addItem(new tsl::elm::ListItem(view.summary));

        auto addRow = [&](const RegionRow& row) {
            auto* item = new tsl::elm::ListItem(row.text);
            // Teleport to this spawner when X is pressed while focused.
            const float tx = row.x, ty = row.y, tz = row.z;
            item->setClickListener([tx, ty, tz](u64 keys) {
                if (keys & HidNpadButton_X) {
                    PlaReader::teleport(tx, ty, tz);
                    return true;
                }
                return false;
            });
            list->addItem(item);
        };

        if (!view.current.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("Shiny now"));
            for (const auto& row : view.current) addRow(row);
        }
        if (!view.near.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("Within reach"));
            for (const auto& row : view.near) addRow(row);
        }
        if (view.current.empty() && view.near.empty())
            list->addItem(new tsl::elm::ListItem("No shinies within reach."));

        list->addItem(new tsl::elm::CategoryHeader(
            "X: teleport    B: back    Y: rescan    " + detach::combo() + ": detach"));
    }

    std::vector<HudLine> hudLines() override {
        auto& s = state();
        if (!s.status.empty()) return {{s.status, false}};

        const RegionView view = buildRegionView(region_);
        std::vector<HudLine> lines{{view.summary, false}};

        if (!view.current.empty()) {
            lines.push_back({"Shiny now", true});
            for (const auto& row : view.current) lines.push_back({row.text, false});
        }
        if (!view.near.empty()) {
            lines.push_back({"Within reach", true});
            for (const auto& row : view.near) lines.push_back({row.text, false});
        }
        if (view.current.empty() && view.near.empty())
            lines.push_back({"No shinies within reach.", false});
        return lines;
    }

    void rebuild() override {
        const PlaRegion region = region_;  // `this` dies inside swapTo
        tsl::swapTo<RegionDetailsGui>(region);
    }

    bool pollForChanges() override { return pollForSpawnerChanges(); }

    bool onInput(u64 keysDown, u64, const HidTouchState&,
                 HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & HidNpadButton_Y) {
            performScan();
            rebuild();
            return true;
        }
        return false;
    }

private:
    PlaRegion region_;
};

class RegionListGui : public DetachableGui {
protected:
    std::string title() override { return "pkTeraRaid"; }

    std::string subtitle() override {
        return detach::attached() ? "Legends Arceus Shiny Scanner" : detach::hint();
    }

    void buildList(tsl::elm::List* list) override {
        ensureScanned();
        auto& s = state();

        if (!s.status.empty()) {
            list->addItem(new tsl::elm::ListItem(s.status));
            list->addItem(new tsl::elm::CategoryHeader("Y: rescan"));
            return;
        }

        list->addItem(new tsl::elm::CategoryHeader("Select region"));

        for (int r = 0; r < (int)PlaRegion::Count; r++) {
            auto region = (PlaRegion)r;
            RegionStats st = statsFor(region);
            char value[64];
            if (st.total == 0)
                std::snprintf(value, sizeof(value), "empty");
            else
                std::snprintf(value, sizeof(value), "%zu now, %zu <%d",
                              st.current, st.near, NEAR_THRESHOLD);

            auto* item = new tsl::elm::ListItem(plaRegionName(region));
            item->setValue(value);
            item->setClickListener([region](u64 keys) {
                if (keys & HidNpadButton_A) {
                    tsl::changeTo<RegionDetailsGui>(region);
                    return true;
                }
                return false;
            });
            list->addItem(item);
        }

        list->addItem(new tsl::elm::CategoryHeader(
            "A: open    Y: rescan    " + detach::combo() + ": detach"));
    }

    std::vector<HudLine> hudLines() override {
        ensureScanned();
        if (!state().status.empty()) return {{state().status, false}};

        std::vector<HudLine> lines{{"Shiny spawners per region", true}};
        for (int r = 0; r < (int)PlaRegion::Count; r++) {
            auto region = (PlaRegion)r;
            RegionStats st = statsFor(region);
            char line[128];
            if (st.total == 0)
                std::snprintf(line, sizeof(line), "%s  -  empty", plaRegionName(region));
            else
                std::snprintf(line, sizeof(line), "%s  -  %zu now, %zu <%d",
                              plaRegionName(region), st.current, st.near, NEAR_THRESHOLD);
            lines.push_back({line, false});
        }
        return lines;
    }

    void rebuild() override { tsl::swapTo<RegionListGui>(); }

    bool pollForChanges() override { return pollForSpawnerChanges(); }

    bool onInput(u64 keysDown, u64, const HidTouchState&,
                 HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & HidNpadButton_Y) {
            performScan();
            rebuild();
            return true;
        }
        return false;
    }
};

class PlaOverlay : public tsl::Overlay {
public:
    void initServices() override {}
    void exitServices() override {
        // libtesla already released the foreground by this point; just make
        // sure the hide suppression detached mode installed is gone.
        tsl::disableHiding = false;
        DmntMem::exit();
    }
    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<RegionListGui>();
    }
};

} // anonymous

int main(int argc, char** argv) {
    return tsl::loop<PlaOverlay>(argc, argv);
}

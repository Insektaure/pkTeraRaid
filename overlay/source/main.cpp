// pkTeraRaid — libultrahand overlay: quick-glance PLA shiny summary.
// Root menu lists the 5 Hisui regions; tapping one opens a detail view
// with current shinies + near-advance spawners. Y rescans memory.

#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "pla/pla_reader.h"
#include "pla/pla_markers.h"
#include "pla/pla_region.h"
#include "dmnt_mem.h"
#include "game_type.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

constexpr int SCAN_MAX_GROUPS = 256;
constexpr int SHINY_SEARCH    = 5000;
constexpr int NEAR_THRESHOLD  = 100;
constexpr size_t TOP_N        = 10;

struct OverlayState {
    PlaReader reader;
    bool      scanned = false;
    std::string status;
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
        out.total++;
        if (sp.shinyAdvance == 0 && sp.firstSpawn.shiny) out.current++;
        else if (sp.shinyAdvance > 0 && sp.shinyAdvance <= NEAR_THRESHOLD) out.near++;
    }
    return out;
}

// Forward decls
class RegionDetailsGui;
class RegionListGui;

class RegionDetailsGui : public tsl::Gui {
public:
    explicit RegionDetailsGui(PlaRegion r) : region_(r) {}

    tsl::elm::Element* createUI() override {
        auto& s = state();
        s.reader.decorate(region_, 1, SHINY_SEARCH);

        auto* frame = new tsl::elm::OverlayFrame(plaRegionName(region_), "pkTeraRaid");
        auto* list  = new tsl::elm::List();

        if (!s.status.empty()) {
            list->addItem(new tsl::elm::ListItem(s.status));
            frame->setContent(list);
            return frame;
        }

        const auto& sps = s.reader.spawners();
        std::vector<int> current, nearN, farN;
        for (int i = 0; i < (int)sps.size(); i++) {
            const auto& sp = sps[i];
            if (sp.region != (int)region_) continue;
            if (sp.shinyAdvance == 0 && sp.firstSpawn.shiny) current.push_back(i);
            else if (sp.shinyAdvance > 0 && sp.shinyAdvance <= NEAR_THRESHOLD) nearN.push_back(i);
            else if (sp.shinyAdvance > NEAR_THRESHOLD) farN.push_back(i);
        }
        std::sort(nearN.begin(), nearN.end(),
                  [&](int a, int b){ return sps[a].shinyAdvance < sps[b].shinyAdvance; });

        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "Current: %zu    <%d adv: %zu    total: %zu",
                      current.size(), NEAR_THRESHOLD, nearN.size(),
                      current.size() + nearN.size() + farN.size());
        list->addItem(new tsl::elm::ListItem(buf));

        auto addRow = [&](int idx) {
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
            auto* item = new tsl::elm::ListItem(row);
            // Teleport to this spawner when X is pressed while focused.
            // Raw marker coords — matches PLA-Live-Map reference impl.
            const float tx = sp.markerX;
            const float ty = sp.markerY;
            const float tz = sp.markerZ;
            item->setClickListener([tx, ty, tz](u64 keys) {
                if (keys & HidNpadButton_X) {
                    PlaReader::teleport(tx, ty, tz);
                    return true;
                }
                return false;
            });
            list->addItem(item);
        };

        if (!current.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("Shiny now"));
            for (int idx : current) addRow(idx);
        }
        if (!nearN.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("Within reach"));
            size_t shown = 0;
            for (int idx : nearN) {
                if (shown++ >= TOP_N) break;
                addRow(idx);
            }
        }
        if (current.empty() && nearN.empty())
            list->addItem(new tsl::elm::ListItem("No shinies within reach."));

        list->addItem(new tsl::elm::CategoryHeader("X: teleport    B: back    Y: rescan"));
        frame->setContent(list);
        return frame;
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&,
                     HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & HidNpadButton_Y) {
            performScan();
            tsl::changeTo<RegionDetailsGui>(region_);
            return true;
        }
        return false;
    }

private:
    PlaRegion region_;
};

class RegionListGui : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        auto& s = state();
        if (!s.scanned) performScan();

        auto* frame = new tsl::elm::OverlayFrame("pkTeraRaid", "Legends Arceus Shiny Scanner");
        auto* list  = new tsl::elm::List();

        if (!s.status.empty()) {
            list->addItem(new tsl::elm::ListItem(s.status));
            list->addItem(new tsl::elm::CategoryHeader("Y: rescan"));
            frame->setContent(list);
            return frame;
        }

        list->addItem(new tsl::elm::CategoryHeader("Select region"));

        for (int r = 0; r < (int)PlaRegion::Count; r++) {
            auto region = (PlaRegion)r;
            RegionStats st = statsFor(region);
            char label[128];
            std::snprintf(label, sizeof(label), "%s", plaRegionName(region));
            char value[64];
            if (st.total == 0)
                std::snprintf(value, sizeof(value), "empty");
            else
                std::snprintf(value, sizeof(value), "%zu now, %zu <%d",
                              st.current, st.near, NEAR_THRESHOLD);

            auto* item = new tsl::elm::ListItem(label);
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

        list->addItem(new tsl::elm::CategoryHeader("A: open    Y: rescan"));
        frame->setContent(list);
        return frame;
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&,
                     HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & HidNpadButton_Y) {
            performScan();
            tsl::changeTo<RegionListGui>();
            return true;
        }
        return false;
    }
};

class PlaOverlay : public tsl::Overlay {
public:
    void initServices() override {}
    void exitServices() override { DmntMem::exit(); }
    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<RegionListGui>();
    }
};

} // anonymous

int main(int argc, char** argv) {
    return tsl::loop<PlaOverlay>(argc, argv);
}

// Detachable overlay Guis
#pragma once

#include <tesla.hpp>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace detach {

constexpr u64 TOGGLE_COMBO = HidNpadButton_L | HidNpadButton_R;

// How often the detached HUD checks whether the game data moved.
constexpr u64 POLL_INTERVAL_NS = 1'000'000'000ULL;

constexpr const char* GLYPH_L = "\uE0E4";
constexpr const char* GLYPH_R = "\uE0E5";

// Attached is the state every session starts in.
inline std::atomic<bool> g_detached{false};

inline bool attached() {
    return !g_detached.load(std::memory_order_acquire);
}

inline void setAttached(bool value) {
    g_detached.store(!value, std::memory_order_release);
    // Detached, the overlay has to survive what normally dismisses it: a touch
    // outside the overlay layer and the HOME/power buttons.
    tsl::disableHiding = !value;
    tsl::hlp::requestForeground(value);
}

inline void toggle() { setAttached(!attached()); }

// True on the frame the combo becomes complete.
inline bool comboPressed(u64 keysDown, u64 keysHeld) {
    return (keysHeld & TOGGLE_COMBO) == TOGGLE_COMBO && (keysDown & TOGGLE_COMBO);
}

inline std::string combo() {
    return std::string(GLYPH_L) + "+" + GLYPH_R;
}

// Shown in the frame subtitle so the way back is always on screen.
inline std::string hint() {
    return attached() ? combo() + " detach" : "DETACHED - " + combo() + " to attach";
}

}  // namespace detach

// One line of the detached HUD.
struct HudLine {
    std::string text;
    bool        header = false;
};

class DetachableGui : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override final {
        const bool isAttached = detach::attached();
        // noClickableItems in detached mode: nothing is focusable, so the frame
        // footer should not advertise an OK button.
        auto* frame = new tsl::elm::OverlayFrame(title(), subtitle(), !isAttached);

        if (isAttached) {
            auto* list = new tsl::elm::List();
            buildList(list);
            frame->setContent(list);
        } else {
            frame->setContent(buildHud());
        }
        return frame;
    }

    // Detached only: poll on an interval and re-render in place when the
    // poll reports the spawner data actually changed.
    void update() override final {
        if (detach::attached()) return;

        const u64 now = armGetSystemTick();
        if (lastPollTick_ != 0 && armTicksToNs(now - lastPollTick_) < detach::POLL_INTERVAL_NS)
            return;
        lastPollTick_ = now;

        if (!pollForChanges() || !hud_) return;

        // A changed seed does not always change what is on screen. Swapping in
        // an identical set of lines would redraw for nothing.
        auto next = hudLines();
        if (next.size() == hud_->size() &&
            std::equal(next.begin(), next.end(), hud_->begin(),
                       [](const HudLine& a, const HudLine& b) {
                           return a.header == b.header && a.text == b.text;
                       }))
            return;

        *hud_ = std::move(next);
    }

    bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos,
                     HidAnalogStickState leftStick, HidAnalogStickState rightStick) override final {
        // Hiding is suppressed while detached, so HOME only raises this flag.
        // We are already out of the foreground — just clear it.
        tsl::homeButtonPressedInGame.store(false, std::memory_order_release);

        if (detach::comboPressed(keysDown, keysHeld)) {
            detach::toggle();
            rebuild();  // destroys `this`
            return true;
        }

        if (!detach::attached()) {
            // The launch combo cannot hide the overlay while hiding is
            // disabled, so make it a second way back to attached mode.
            const u64 launch = tsl::cfg::launchCombo;
            if (launch != 0 && (keysHeld & launch) == launch && (keysDown & launch)) {
                detach::setAttached(true);
                rebuild();  // destroys `this`
                return true;
            }
            // Consume everything else: this stops directional navigation and
            // keeps B from popping the Gui (which would close the overlay).
            return true;
        }

        return onInput(keysDown, keysHeld, touchPos, leftStick, rightStick);
    }

protected:
    virtual std::string title()    = 0;
    virtual std::string subtitle() = 0;

    // Interactive body, built when attached.
    virtual void buildList(tsl::elm::List* list) = 0;

    // The same information as flat text, drawn when detached.
    virtual std::vector<HudLine> hudLines() = 0;

    // Re-create this Gui in place (tsl::swapTo) so it renders in the current
    // mode. Destroys `this` — never touch members after calling it.
    virtual void rebuild() = 0;

    virtual bool onInput(u64, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) {
        return false;
    }

    // Called on the detached poll interval. Return true only when the data
    // really changed - returning true costs a full hudLines() rebuild.
    virtual bool pollForChanges() { return false; }

private:
    // Shared with the drawer so a refresh is a vector assignment rather than a
    // Gui rebuild. update() and draw() run on the same thread, in that order.
    std::shared_ptr<std::vector<HudLine>> hud_;
    u64 lastPollTick_ = 0;

    tsl::elm::Element* buildHud() {
        hud_ = std::make_shared<std::vector<HudLine>>(hudLines());
        auto lines = hud_;
        return new tsl::elm::CustomDrawer(
            [lines](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32, s32 h) {
                constexpr s32 ROW_HEIGHT    = 30;
                constexpr s32 HEADER_HEIGHT = 27;
                constexpr s32 HEADER_GAP    = 8;

                s32 lineY = y;
                for (const auto& line : *lines) {
                    if (line.header) lineY += HEADER_GAP;
                    if (lineY + ROW_HEIGHT > y + h) break;
                    renderer->drawString(line.text, false, x, lineY + 20,
                                         line.header ? 15 : 19,
                                         line.header ? tsl::headerTextColor : tsl::defaultTextColor);
                    lineY += line.header ? HEADER_HEIGHT : ROW_HEIGHT;
                }
            });
    }
};

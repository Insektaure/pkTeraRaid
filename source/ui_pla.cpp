#include "ui.h"
#include "pla/pla_markers.h"
#include <switch.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

static const char* NATURE_NAMES[25] = {
    "Hardy","Lonely","Brave","Adamant","Naughty",
    "Bold","Docile","Relaxed","Impish","Lax",
    "Timid","Hasty","Serious","Jolly","Naive",
    "Modest","Mild","Quiet","Bashful","Rash",
    "Calm","Gentle","Sassy","Careful","Quirky"
};

void UI::runPla(const std::string& basePath) {
    basePath_ = basePath;
    liveMode_ = true;
    selectedVersion_ = GameVersion::LegendsArceus;

    showWorking("Loading Hisui maps...");
    std::string mapDir = "romfs:/maps/";
    auto loadMap = [&](const char* name) -> SDL_Texture* {
        std::string path = mapDir + name;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return nullptr;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        return tex;
    };
    for (int r = 0; r < (int)PlaRegion::Count; r++) {
        if (!plaMaps_[r])
            plaMaps_[r] = loadMap(plaRegionMapFile((PlaRegion)r));
    }

    showWorking("Scanning spawners...");
    if (!plaReader_.readLive(256)) {
        showMessageAndWait("Error",
            "Failed to read spawner array.\n"
            "Make sure Pokemon Legends: Arceus\n"
            "1.1.0 or 1.1.1 is running\n"
            "and dmnt:cht can attach.");
        return;
    }
    plaOutbreak_ = plaReader_.readOutbreak();
    plaTab_ = 0;
    plaReader_.decorate((PlaRegion)plaTab_);
    plaCursor_ = 0;
    plaScroll_ = 0;
    rebuildPlaFilteredList();

    dirty_ = true;
    bool running = true;
    while (running) {
        bool aboutBefore = showAbout_;
        if (showAbout_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                        showAbout_ = false;
                }
            }
        } else {
            handlePlaViewInput(running);
        }
        if (showAbout_ != aboutBefore) markDirty();

        if (dirty_) {
            drawPlaViewFrame();
            if (showAbout_) drawAboutPopup();
            SDL_RenderPresent(renderer_);
            dirty_ = false;
        }
        SDL_Delay(16);
    }
}

void UI::rebuildPlaFilteredList() {
    plaFiltered_.clear();
    auto& sps = plaReader_.spawners();
    plaFiltered_.reserve(sps.size());
    for (int i = 0; i < (int)sps.size(); i++) {
        const auto& s = sps[i];
        if (s.region != plaTab_) continue;
        if (plaActiveOnly_ && !s.active) continue;
        if (plaShinyOnly_ && !(s.shinyAdvance == 0 && s.firstSpawn.shiny)) continue;
        plaFiltered_.push_back(i);
    }
    // Sort by shiny advance ASC: 0 (current) first, then 1..N, then -1 (no shiny) last.
    std::sort(plaFiltered_.begin(), plaFiltered_.end(), [&sps](int a, int b) {
        int sa = sps[a].shinyAdvance, sb = sps[b].shinyAdvance;
        bool ua = (sa < 0), ub = (sb < 0);
        if (ua != ub) return !ua;               // unknowns sink
        if (!ua && sa != sb) return sa < sb;    // ascending among knowns
        return sps[a].groupId < sps[b].groupId; // stable by group_id
    });
    if (plaCursor_ >= (int)plaFiltered_.size())
        plaCursor_ = std::max(0, (int)plaFiltered_.size() - 1);
    if (plaScroll_ > plaCursor_) plaScroll_ = plaCursor_;
}

void UI::drawPlaViewFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    char title[128];
    snprintf(title, sizeof(title), "pkTeraRaid - %s",
             gameDisplayNameOf(selectedVersion_));
    drawText(title, 10, 10, COLOR_TERA, font_);

    int tw, th;
    TTF_SizeUTF8(font_, "Live Mode", &tw, &th);
    drawText("Live Mode", (SCREEN_W - tw) / 2, 10, COLOR_SHINY, font_);

    drawPlaMapPanel();
    drawPlaListPanel();

    if (plaShowDetail_ && !plaFiltered_.empty()) {
        int idx = plaFiltered_[plaCursor_];
        drawPlaDetailPopup(plaReader_.spawners()[idx]);
    }

    drawStatusBar(
        "D-Pad: Nav  ZL/ZR: Page  A: Detail  X: Active-only  Y: Shiny-only  L/R: Region  -: About  +: Quit",
        "Live Mode - Pokemon Legends: Arceus");
}

void UI::drawPlaMapTabs(int x, int y, int w) {
    constexpr int NUM_TABS = (int)PlaRegion::Count;
    int tabW = w / NUM_TABS;
    const char* short_[] = {"Obsidian", "Crimson", "Cobalt", "Coronet", "Alabaster"};
    for (int i = 0; i < NUM_TABS; i++) {
        int tx = x + i * tabW;
        bool active = (i == plaTab_);
        if (active) {
            drawRect(tx, y, tabW, TAB_H, {60, 60, 90, 255});
            drawTextCentered(short_[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT, fontSmall_);
        } else {
            drawRect(tx, y, tabW, TAB_H, {35, 35, 50, 255});
            drawTextCentered(short_[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT_DIM, fontSmall_);
        }
        if (i < NUM_TABS - 1) {
            SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 255);
            SDL_RenderDrawLine(renderer_, tx + tabW, y + 4, tx + tabW, y + TAB_H - 4);
        }
    }
}

void UI::drawPlaMapPanel() {
    drawRect(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W, MAP_PANEL_H, COLOR_PANEL_BG);
    drawPlaMapTabs(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W);

    int areaX = MAP_PANEL_X + 5;
    int areaY = MAP_PANEL_Y + TAB_H + 5;
    int areaW = MAP_PANEL_W - 10;
    int areaH = MAP_PANEL_H - TAB_H - 10;

    SDL_Texture* mapTex = plaMaps_[plaTab_];
    const auto& b = PlaMarkers::BOUNDS[plaTab_];
    int imgW = b.mapImgW, imgH = b.mapImgH;

    float scaleX = (float)areaW / imgW;
    float scaleY = (float)areaH / imgH;
    float scale = std::min(scaleX, scaleY);
    int mapW = (int)(imgW * scale);
    int mapH = (int)(imgH * scale);
    int mapX = areaX + (areaW - mapW) / 2;
    int mapY = areaY + (areaH - mapH) / 2;

    if (mapTex) {
        SDL_Rect dst = {mapX, mapY, mapW, mapH};
        SDL_RenderCopy(renderer_, mapTex, nullptr, &dst);
    } else {
        drawRect(mapX, mapY, mapW, mapH, {25, 30, 40, 255});
        drawTextCentered("Map image missing —",
                         mapX + mapW / 2, mapY + mapH / 2 - 12,
                         COLOR_TEXT_DIM, fontSmall_);
        drawTextCentered("drop into romfs/maps/",
                         mapX + mapW / 2, mapY + mapH / 2 + 10,
                         COLOR_TEXT_DIM, fontSmall_);
    }

    // World->screen projection using region bounds.
    float wx = b.maxX - b.minX;
    float wz = b.maxZ - b.minZ;
    if (wx < 1.0f) wx = 1.0f;
    if (wz < 1.0f) wz = 1.0f;

    auto& sps = plaReader_.spawners();
    for (int fi = 0; fi < (int)plaFiltered_.size(); fi++) {
        int idx = plaFiltered_[fi];
        const auto& s = sps[idx];
        if (s.region != plaTab_) continue;
        float u = (s.markerX - b.minX) / wx;
        float v = (s.markerZ - b.minZ) / wz;
        int sx = mapX + (int)(u * mapW);
        int sy = mapY + (int)(v * mapH);
        if (sx < mapX || sx > mapX + mapW || sy < mapY || sy > mapY + mapH) continue;

        bool currentShiny = (s.shinyAdvance == 0 && s.firstSpawn.shiny);
        bool selected = (fi == plaCursor_);
        int radius = selected ? 7 : 5;

        SDL_Color dot = COLOR_MAP_DOT;
        if (currentShiny)     dot = COLOR_SHINY;
        else if (!s.active)   dot = {90, 90, 100, 255};
        fillCircle(sx, sy, radius, dot);

        if (selected) {
            SDL_SetRenderDrawColor(renderer_, COLOR_CURSOR.r, COLOR_CURSOR.g, COLOR_CURSOR.b, 255);
            int rr = radius + 3;
            for (int angle = 0; angle < 360; angle += 5) {
                double rad = angle * 3.14159265 / 180.0;
                for (int t = 0; t <= 1; t++) {
                    int ox = sx + (int)((rr + t) * std::cos(rad));
                    int oy = sy + (int)((rr + t) * std::sin(rad));
                    SDL_RenderDrawPoint(renderer_, ox, oy);
                }
            }
        }
    }

    // Mini outbreak badge in the bottom-left of the map area
    if (plaOutbreak_.present) {
        int bx = mapX + 8, by = mapY + mapH - 28;
        drawRect(bx, by, 200, 22, {40, 40, 60, 220});
        char buf[64];
        snprintf(buf, sizeof(buf), "Outbreak: grp %d x%u",
                 plaOutbreak_.groupId, (unsigned)plaOutbreak_.spawnCount);
        drawText(buf, bx + 6, by + 3, COLOR_SHINY, fontSmall_);
    }
}

void UI::drawPlaListPanel() {
    drawRect(LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_W, LIST_PANEL_H, COLOR_PANEL_BG);

    auto& sps = plaReader_.spawners();
    int count = (int)plaFiltered_.size();

    int activeCount = 0, shinyCount = 0;
    for (int fi : plaFiltered_) {
        const auto& s = sps[fi];
        if (s.active) activeCount++;
        if (s.shinyAdvance == 0 && s.firstSpawn.shiny) shinyCount++;
    }
    char header[128];
    snprintf(header, sizeof(header), "%s: %d  (%d active, %d shiny)",
             plaRegionName((PlaRegion)plaTab_), count, activeCount, shinyCount);
    drawText(header, LIST_PANEL_X + 10, LIST_PANEL_Y + 5, COLOR_TEXT, fontSmall_);

    int listY = LIST_PANEL_Y + 28;
    int listH = LIST_PANEL_H - 28;
    int visibleRows = std::max(1, listH / 60);
    int rowH = listH / visibleRows;

    if (plaCursor_ < plaScroll_) plaScroll_ = plaCursor_;
    if (plaCursor_ >= plaScroll_ + visibleRows)
        plaScroll_ = plaCursor_ - visibleRows + 1;

    for (int vi = 0; vi < visibleRows && (plaScroll_ + vi) < count; vi++) {
        int fi = plaScroll_ + vi;
        int idx = plaFiltered_[fi];
        int rowY = listY + vi * rowH;
        drawPlaRow(LIST_PANEL_X + 5, rowY, LIST_PANEL_W - 10,
                   sps[idx], fi == plaCursor_, rowH);
    }

    if (count > visibleRows) {
        int sbX = LIST_PANEL_X + LIST_PANEL_W - 6;
        int sbH = listH;
        int thumbH = std::max(20, sbH * visibleRows / count);
        int thumbY = listY + (sbH - thumbH) * plaScroll_ / (count - visibleRows);
        drawRect(sbX, listY, 4, sbH, {40, 40, 55, 255});
        drawRect(sbX, thumbY, 4, thumbH, {100, 100, 130, 255});
    }
}

void UI::drawPlaRow(int x, int y, int w, const PlaSpawner& s, bool selected, int rowH) {
    bool currentShiny = (s.shinyAdvance == 0 && s.firstSpawn.shiny);
    int rh = rowH - 2;

    if (currentShiny) {
        drawRect(x, y, w, rh, selected ? SDL_Color{180,155,40,255} : SDL_Color{150,130,30,255});
        if (selected) drawRectOutline(x, y, w, rh, COLOR_CURSOR, 2);
    } else {
        drawRect(x, y, w, rh, selected ? SDL_Color{60,70,100,255} : SDL_Color{40,40,55,255});
        if (selected) drawRectOutline(x, y, w, rh, COLOR_CURSOR, 2);
    }

    SDL_Color textMain = currentShiny ? SDL_Color{30,25,10,255} : COLOR_TEXT;
    SDL_Color textDim  = currentShiny ? SDL_Color{70,60,20,255} : COLOR_TEXT_DIM;
    SDL_Color textSeed = currentShiny ? SDL_Color{90,75,25,255} : SDL_Color{100,100,120,255};

    int line1Y = y + rh / 2 - 22;
    if (line1Y < y + 2) line1Y = y + 2;
    int line2Y = y + rh / 2 + 4;

    char buf[128];
    if (s.speciesName) {
        snprintf(buf, sizeof(buf), "%s%s",
                 s.alpha ? "α " : "", s.speciesName);
    } else {
        snprintf(buf, sizeof(buf), "Group %d", s.groupId);
    }
    SDL_Color nameColor = s.alpha ? SDL_Color{220,120,60,255} : textMain;
    if (currentShiny) nameColor = SDL_Color{30,25,10,255};
    drawText(buf, x + 10, line1Y, nameColor, font_);

    if (s.active) drawText("[active]", x + 200, line1Y, SDL_Color{140,200,140,255}, fontSmall_);

    // Shiny / advance info
    if (s.shinyAdvance == 0 && s.firstSpawn.shiny) {
        drawTextRight("Shiny now!", x + w - 10, line1Y, SDL_Color{120,40,0,255}, font_);
    } else if (s.shinyAdvance > 0) {
        snprintf(buf, sizeof(buf), "Shiny in %d", s.shinyAdvance);
        drawTextRight(buf, x + w - 10, line1Y, COLOR_SHINY, fontSmall_);
    } else {
        drawTextRight("No shiny <5000", x + w - 10, line1Y, textDim, fontSmall_);
    }

    // Line 2: IVs + nature + seed
    char ivs[48];
    snprintf(ivs, sizeof(ivs), "%d/%d/%d/%d/%d/%d",
             s.firstSpawn.ivs[0], s.firstSpawn.ivs[1], s.firstSpawn.ivs[2],
             s.firstSpawn.ivs[3], s.firstSpawn.ivs[4], s.firstSpawn.ivs[5]);
    drawText(ivs, x + 10, line2Y, textDim, fontSmall_);

    drawText(NATURE_NAMES[s.firstSpawn.nature % 25], x + 200, line2Y, textDim, fontSmall_);

    char seedBuf[24];
    snprintf(seedBuf, sizeof(seedBuf), "%016lX", (unsigned long)s.groupSeed);
    drawTextRight(seedBuf, x + w - 10, line2Y, textSeed, fontSmall_);
}

void UI::drawPlaDetailPopup(const PlaSpawner& s) {
    constexpr int POP_W = 580, POP_H = 420;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(renderer_, &full);

    drawRect(px, py, POP_W, POP_H, {35,35,50,255});
    drawRectOutline(px, py, POP_W, POP_H, COLOR_TERA, 2);

    int lx = px + 20;
    int y  = py + 15;
    int lh = 24;

    char buf[128];
    if (s.speciesName) {
        snprintf(buf, sizeof(buf), "%s%s  (Group %d)",
                 s.alpha ? "α " : "", s.speciesName, s.groupId);
    } else {
        snprintf(buf, sizeof(buf), "Group %d  %s",
                 s.groupId, s.active ? "(active)" : "(inactive)");
    }
    SDL_Color titleColor = s.alpha ? SDL_Color{240,160,80,255} : COLOR_TERA;
    drawText(buf, lx, y, titleColor, fontLarge_); y += 40;

    SDL_SetRenderDrawColor(renderer_, 70, 70, 90, 255);
    SDL_RenderDrawLine(renderer_, lx, y, px + POP_W - 20, y);
    y += 10;

    drawText("Generator Seed:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    snprintf(buf, sizeof(buf), "%016lX", (unsigned long)s.generatorSeed);
    drawText(buf, lx + 170, y, COLOR_TEXT, fontSmall_); y += lh;

    drawText("Group Seed:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    snprintf(buf, sizeof(buf), "%016lX", (unsigned long)s.groupSeed);
    drawText(buf, lx + 170, y, COLOR_TEXT, fontSmall_); y += lh;

    if (s.active) {
        drawText("Position:", lx, y, COLOR_TEXT_DIM, fontSmall_);
        snprintf(buf, sizeof(buf), "%.1f, %.1f, %.1f", s.livePosX, s.livePosY, s.livePosZ);
        drawText(buf, lx + 170, y, COLOR_TEXT, fontSmall_); y += lh;
    }
    y += 6;

    drawText("First fixed spawn:", lx, y, COLOR_SHINY, font_); y += 28;

    drawText("EC:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    snprintf(buf, sizeof(buf), "%08X", s.firstSpawn.ec);
    drawText(buf, lx + 170, y, COLOR_TEXT, fontSmall_); y += lh;

    drawText("PID:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    snprintf(buf, sizeof(buf), "%08X", s.firstSpawn.pid);
    drawText(buf, lx + 170, y, s.firstSpawn.shiny ? COLOR_SHINY : COLOR_TEXT, fontSmall_);
    y += lh;

    drawText("Nature:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    drawText(NATURE_NAMES[s.firstSpawn.nature % 25], lx + 170, y, COLOR_TEXT, fontSmall_);
    y += lh;

    drawText("IVs:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    const char* statNames[] = {"HP","Atk","Def","SpA","SpD","Spe"};
    int ivx = lx + 170;
    for (int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%s %d", statNames[i], s.firstSpawn.ivs[i]);
        SDL_Color c = (s.firstSpawn.ivs[i] == 31) ? COLOR_SHINY :
                      (s.firstSpawn.ivs[i] == 0)  ? COLOR_RED   : COLOR_TEXT;
        drawText(buf, ivx + (i % 3) * 70, y + (i / 3) * 20, c, fontSmall_);
    }
    y += lh * 2;

    drawText("Shiny:", lx, y, COLOR_TEXT_DIM, fontSmall_);
    if (s.shinyAdvance == 0 && s.firstSpawn.shiny) {
        drawText("Current (advance 0)", lx + 170, y, COLOR_SHINY, fontSmall_);
    } else if (s.shinyAdvance > 0) {
        snprintf(buf, sizeof(buf), "in %d advance%s",
                 s.shinyAdvance, s.shinyAdvance == 1 ? "" : "s");
        drawText(buf, lx + 170, y, COLOR_SHINY, fontSmall_);
    } else {
        drawText("None within 5000", lx + 170, y, COLOR_TEXT_DIM, fontSmall_);
    }

    drawTextRight("B: Close", px + POP_W - 20, py + POP_H - 35, COLOR_TEXT_DIM, fontSmall_);
}

void UI::handlePlaViewInput(bool& running) {
    int count = (int)plaFiltered_.size();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { running = false; return; }

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            markDirty();
            if (plaShowDetail_) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                    plaShowDetail_ = false;
                continue;
            }
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (count > 0) plaCursor_ = (plaCursor_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (count > 0) plaCursor_ = (plaCursor_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A
                    if (count > 0) plaShowDetail_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X
                    plaActiveOnly_ = !plaActiveOnly_;
                    plaCursor_ = 0; plaScroll_ = 0;
                    rebuildPlaFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_X: // Switch Y
                    plaShinyOnly_ = !plaShinyOnly_;
                    plaCursor_ = 0; plaScroll_ = 0;
                    rebuildPlaFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    plaTab_ = (plaTab_ + (int)PlaRegion::Count - 1) % (int)PlaRegion::Count;
                    plaReader_.decorate((PlaRegion)plaTab_);
                    plaCursor_ = 0; plaScroll_ = 0;
                    rebuildPlaFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    plaTab_ = (plaTab_ + 1) % (int)PlaRegion::Count;
                    plaReader_.decorate((PlaRegion)plaTab_);
                    plaCursor_ = 0; plaScroll_ = 0;
                    rebuildPlaFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    showAbout_ = true; break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false; break;
            }
        }

    }

    if (!plaShowDetail_ && stickDirY_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay && count > 0) {
            markDirty();
            if (stickDirY_ < 0) plaCursor_ = (plaCursor_ + count - 1) % count;
            else                plaCursor_ = (plaCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }

    // ZL/ZR page scroll (hold to repeat)
    if (!plaShowDetail_ && count > 0) {
        int trig = pollTrigger();
        if (trig != 0) {
            int nc = plaCursor_ + trig * LIST_PAGE_STEP;
            if (nc < 0) nc = 0;
            if (nc > count - 1) nc = count - 1;
            if (nc != plaCursor_) { plaCursor_ = nc; markDirty(); }
        }
    }
}

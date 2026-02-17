#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// --- SV Raid View ---

void UI::rebuildFilteredList() {
    filteredIndices_.clear();
    auto& raids = reader_.raids();
    for (int i = 0; i < (int)raids.size(); i++) {
        if (raids[i].map == currentMap_)
            filteredIndices_.push_back(i);
    }
    if (raidCursor_ >= (int)filteredIndices_.size())
        raidCursor_ = std::max(0, (int)filteredIndices_.size() - 1);
}

// worldToScreen is handled inline in drawMapPanel using dynamic bounds from filtered raids

void UI::drawMapPanel() {
    // Map panel background
    drawRect(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W, MAP_PANEL_H, COLOR_PANEL_BG);

    // Map tabs
    drawMapTabs(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W);

    // Map image area (available space)
    int areaX = MAP_PANEL_X + 5;
    int areaY = MAP_PANEL_Y + TAB_H + 5;
    int areaW = MAP_PANEL_W - 10;
    int areaH = MAP_PANEL_H - TAB_H - 10;

    // Draw map texture preserving aspect ratio (maps are square)
    SDL_Texture* mapTex = nullptr;
    switch (currentMap_) {
        case TeraRaidMapParent::Paldea:    mapTex = mapPaldea_; break;
        case TeraRaidMapParent::Kitakami:   mapTex = mapKitakami_; break;
        case TeraRaidMapParent::Blueberry:  mapTex = mapBlueberry_; break;
    }

    // Fit square map into available area, centered
    int mapSide = std::min(areaW, areaH);
    int mapX = areaX + (areaW - mapSide) / 2;
    int mapY = areaY + (areaH - mapSide) / 2;
    int mapW = mapSide;
    int mapH = mapSide;

    if (mapTex) {
        SDL_Rect dst = {mapX, mapY, mapW, mapH};
        SDL_RenderCopy(renderer_, mapTex, nullptr, &dst);
    } else {
        drawRect(mapX, mapY, mapW, mapH, {25, 35, 25, 255});
    }

    // Convert world coordinates to map pixel positions using RaidCrawler formulas
    // (from Tera-Finder ImagesUtil.cs, originally from RaidCrawler by Lincoln/Lego/Archit)
    auto& raids = reader_.raids();

    if (filteredIndices_.empty()) {
        drawTextCentered("No raid locations", mapX + mapW / 2, mapY + mapH / 2, COLOR_TEXT_DIM, font_);
        return;
    }

    auto worldToMap = [&](float wx, float wz, int& sx, int& sy) {
        // Formulas from RaidCrawler MapMagic: ConvertX = (512/Scale)*x, ConvertZ = (512/Scale)*(z+Offset)
        double refX, refY;
        constexpr double REF = 512.0;

        switch (currentMap_) {
            case TeraRaidMapParent::Paldea:
                refX = (REF / 5000.0) * wx;
                refY = (REF / 5000.0) * (wz + 5500.0);
                break;
            case TeraRaidMapParent::Kitakami:
            case TeraRaidMapParent::Blueberry:
            default:
                refX = (REF / 2000.0) * wx;
                refY = (REF / 2000.0) * (wz + 2000.0);
                break;
        }

        sx = mapX + (int)(refX * mapW / REF);
        sy = mapY + (int)(refY * mapH / REF);
    };

    // Draw raid dots
    for (int fi = 0; fi < (int)filteredIndices_.size(); fi++) {
        int idx = filteredIndices_[fi];
        if (!raids[idx].hasCoord) continue;

        auto& c = raids[idx].coord;
        int sx, sy;
        worldToMap(c.x, c.z, sx, sy);

        bool isSelected = (fi == raidCursor_);
        int radius = isSelected ? 7 : 5;

        // Color by star rating / content
        SDL_Color dotColor;
        if (raids[idx].content == RaidContent::Black) {
            dotColor = COLOR_MAP_DOT_6;
        } else if (raids[idx].details.shiny != TeraShiny::No) {
            dotColor = COLOR_SHINY;
        } else {
            dotColor = COLOR_MAP_DOT;
        }

        fillCircle(sx, sy, radius, dotColor);
        if (isSelected) {
            // Draw selection ring outline around the dot
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

        // Black outline for 6-star
        if (raids[idx].content == RaidContent::Black) {
            SDL_SetRenderDrawColor(renderer_, 200, 200, 200, 255);
            // Simple outline: draw circle border
            for (int angle = 0; angle < 360; angle += 15) {
                double rad = angle * 3.14159265 / 180.0;
                int ox = sx + (int)((radius + 1) * std::cos(rad));
                int oy = sy + (int)((radius + 1) * std::sin(rad));
                SDL_RenderDrawPoint(renderer_, ox, oy);
            }
        }
    }
}

void UI::drawMapTabs(int x, int y, int w) {
    constexpr int NUM_TABS = 3;
    int tabW = w / NUM_TABS;

    const char* names[] = {"Paldea", "Kitakami", "Blueberry"};
    TeraRaidMapParent maps[] = {
        TeraRaidMapParent::Paldea,
        TeraRaidMapParent::Kitakami,
        TeraRaidMapParent::Blueberry
    };

    for (int i = 0; i < NUM_TABS; i++) {
        int tx = x + i * tabW;
        bool active = (maps[i] == currentMap_);

        if (active) {
            drawRect(tx, y, tabW, TAB_H, {60, 60, 90, 255});
            drawTextCentered(names[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT, fontSmall_);
        } else {
            drawRect(tx, y, tabW, TAB_H, {35, 35, 50, 255});
            drawTextCentered(names[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT_DIM, fontSmall_);
        }

        // Tab separator
        if (i < NUM_TABS - 1) {
            SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 255);
            SDL_RenderDrawLine(renderer_, tx + tabW, y + 4, tx + tabW, y + TAB_H - 4);
        }
    }
}

void UI::drawListPanel() {
    drawRect(LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_W, LIST_PANEL_H, COLOR_PANEL_BG);

    auto& raids = reader_.raids();
    int count = (int)filteredIndices_.size();

    // Header
    int headerY = LIST_PANEL_Y + 5;
    char header[64];
    snprintf(header, sizeof(header), "Raids: %d", count);
    drawText(header, LIST_PANEL_X + 10, headerY, COLOR_TEXT, fontSmall_);

    // Trainer info
    char tidBuf[32];
    snprintf(tidBuf, sizeof(tidBuf), "TID: %06u", reader_.trainerID32() % 1000000);
    drawTextRight(tidBuf, LIST_PANEL_X + LIST_PANEL_W - 10, headerY, COLOR_TEXT_DIM, fontSmall_);

    int listY = LIST_PANEL_Y + 28;
    int listH = LIST_PANEL_H - 28;

    // Compute row height to fill the panel exactly (no bottom gap)
    // Aim for ~10 visible rows, minimum 50px per row
    int visibleRows = std::max(1, listH / 60);
    int rowH = listH / visibleRows;

    // Ensure cursor is visible
    if (raidCursor_ < raidScroll_) raidScroll_ = raidCursor_;
    if (raidCursor_ >= raidScroll_ + visibleRows)
        raidScroll_ = raidCursor_ - visibleRows + 1;

    for (int vi = 0; vi < visibleRows && (raidScroll_ + vi) < count; vi++) {
        int fi = raidScroll_ + vi;
        int idx = filteredIndices_[fi];
        int rowY = listY + vi * rowH;
        bool selected = (fi == raidCursor_);

        drawRaidRow(LIST_PANEL_X + 5, rowY, LIST_PANEL_W - 10, raids[idx], selected, rowH);
    }

    // Scrollbar
    if (count > visibleRows) {
        int sbX = LIST_PANEL_X + LIST_PANEL_W - 6;
        int sbH = listH;
        int thumbH = std::max(20, sbH * visibleRows / count);
        int thumbY = listY + (sbH - thumbH) * raidScroll_ / (count - visibleRows);
        drawRect(sbX, listY, 4, sbH, {40, 40, 55, 255});
        drawRect(sbX, thumbY, 4, thumbH, {100, 100, 130, 255});
    }
}

void UI::drawRaidRow(int x, int y, int w, const RaidInfo& raid, bool selected, int rowH) {
    bool isShiny = raid.details.shiny != TeraShiny::No;
    int rh = rowH - 2; // inner height with gap

    // Background: gold for shiny, normal otherwise
    if (isShiny) {
        if (selected) {
            drawRect(x, y, w, rh, {180, 155, 40, 255});
            drawRectOutline(x, y, w, rh, COLOR_CURSOR, 2);
        } else {
            drawRect(x, y, w, rh, {150, 130, 30, 255});
        }
    } else {
        if (selected) {
            drawRect(x, y, w, rh, {60, 70, 100, 255});
            drawRectOutline(x, y, w, rh, COLOR_CURSOR, 2);
        } else {
            drawRect(x, y, w, rh, {40, 40, 55, 255});
        }
    }

    // Dark text colors for shiny rows (gold bg), normal colors otherwise
    SDL_Color textMain = isShiny ? SDL_Color{30, 25, 10, 255} : COLOR_TEXT;
    SDL_Color textDim  = isShiny ? SDL_Color{70, 60, 20, 255} : COLOR_TEXT_DIM;
    SDL_Color textStar = isShiny ? SDL_Color{100, 60, 0, 255} : COLOR_SHINY;
    SDL_Color textSeed = isShiny ? SDL_Color{90, 75, 25, 255} : SDL_Color{100, 100, 120, 255};

    // Sprite on the left (fit to box, preserve aspect ratio)
    int spriteSize = std::min(LIST_SPRITE_SIZE, rh - 4);
    int spriteX = x + 4;
    int spriteY = y + (rh - spriteSize) / 2;
    SDL_Texture* sprite = getSprite(raid.details.species);
    if (sprite) {
        int texW, texH;
        SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
        float scale = std::min((float)spriteSize / texW, (float)spriteSize / texH);
        int dstW = (int)(texW * scale);
        int dstH = (int)(texH * scale);
        SDL_Rect dst = {spriteX + (spriteSize - dstW) / 2,
                        spriteY + (spriteSize - dstH) / 2, dstW, dstH};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    int textX = x + spriteSize + 12;

    // Fixed column offsets (relative to textX) for alignment across rows
    // Line 1: Stars | Species | Level | Tera Type | Shiny
    constexpr int COL_STARS    = 0;
    constexpr int COL_SPECIES  = 55;
    constexpr int COL_LEVEL    = 210;
    constexpr int COL_TERA     = 270;
    constexpr int COL_SHINY    = 350;
    // Line 2: IVs | Nature | Ability | Seed (right-aligned)
    constexpr int COL_IVS      = 0;
    constexpr int COL_NATURE   = 165;  // fits worst case "31/31/31/31/31/31" + padding
    constexpr int COL_ABILITY  = 260;

    // First line: vertically centered in top half
    int line1Y = y + rh / 2 - 22;
    if (line1Y < y + 2) line1Y = y + 2;

    std::string stars = getStarString(raid.details.stars);
    drawText(stars, textX + COL_STARS, line1Y, textStar, font_);

    std::string species = getSpeciesName(raid.details.species);
    if (raid.details.form > 0)
        species += " (F" + std::to_string(raid.details.form) + ")";
    drawText(species, textX + COL_SPECIES, line1Y, textMain, font_);

    char lvl[16];
    snprintf(lvl, sizeof(lvl), "Lv.%d", raid.details.level);
    drawText(lvl, textX + COL_LEVEL, line1Y, textDim, font_);

    std::string teraStr = getTypeName(raid.details.teraType);
    SDL_Color teraCol = isShiny ? SDL_Color{50, 30, 10, 255} : getTypeColor(raid.details.teraType);
    drawText(teraStr, textX + COL_TERA, line1Y, teraCol, font_);

    if (isShiny) {
        drawText("Shiny!", textX + COL_SHINY, line1Y, {120, 40, 0, 255}, font_);
    }

    // Second line: vertically centered in bottom half
    int line2Y = y + rh / 2 + 4;

    char ivBuf[48];
    snprintf(ivBuf, sizeof(ivBuf), "%d/%d/%d/%d/%d/%d",
        raid.details.ivs[0], raid.details.ivs[1], raid.details.ivs[2],
        raid.details.ivs[3], raid.details.ivs[4], raid.details.ivs[5]);
    drawText(ivBuf, textX + COL_IVS, line2Y, textDim, fontSmall_);

    drawText(getNatureName(raid.details.nature), textX + COL_NATURE, line2Y, textDim, fontSmall_);

    drawText(getAbilityName(raid.details.ability), textX + COL_ABILITY, line2Y, textDim, fontSmall_);

    char seedBuf[16];
    snprintf(seedBuf, sizeof(seedBuf), "%08X", raid.details.seed);
    drawTextRight(seedBuf, x + w - 8, line2Y, textSeed, fontSmall_);
}

void UI::drawRaidViewFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Title bar
    const char* gameName = (selectedVersion_ == GameVersion::Scarlet) ? "Scarlet" : "Violet";
    char title[128];
    if (!liveMode_ && selectedProfile_ >= 0 && selectedProfile_ < (int)account_.profiles().size()) {
        snprintf(title, sizeof(title), "pkTeraRaid - %s - %s", gameName,
                 account_.profiles()[selectedProfile_].nickname.c_str());
    } else {
        snprintf(title, sizeof(title), "pkTeraRaid - %s", gameName);
    }
    drawText(title, 10, 10, COLOR_TERA, font_);

    if (liveMode_) {
        int tw, th;
        TTF_SizeUTF8(font_, "Live Mode", &tw, &th);
        drawText("Live Mode", (SCREEN_W - tw) / 2, 10, COLOR_SHINY, font_);
    }

    // Progress indicator
    const char* progNames[] = {"Beginning", "Tera Raids", "3 Stars", "4 Stars", "5 Stars", "6 Stars"};
    int progIdx = (int)reader_.progress();
    if (progIdx >= 0 && progIdx <= 5) {
        char progBuf[48];
        snprintf(progBuf, sizeof(progBuf), "Progress: %s", progNames[progIdx]);
        drawTextRight(progBuf, SCREEN_W - 10, 12, COLOR_TEXT_DIM, fontSmall_);
    }

    drawMapPanel();
    drawListPanel();

    if (showDetail_ && !filteredIndices_.empty()) {
        int idx = filteredIndices_[raidCursor_];
        drawDetailPopup(reader_.raids()[idx]);
    }

    if (liveMode_) {
        std::string liveStatus = "Live Mode - ";
        liveStatus += gameDisplayNameOf(selectedVersion_);
        liveStatus += "  |  D-Pad:Navigate  A:Detail  L/R:Map Tab  -:About  +:Quit";
        drawStatusBar(liveStatus);
    } else {
        drawStatusBar("D-Pad:Navigate  A:Detail  B:Back  L/R:Map Tab  -:About  +:Quit");
    }
}

void UI::drawDetailPopup(const RaidInfo& raid) {
    constexpr int POP_W = 620, POP_H = 420;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    // Dim background
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
    SDL_Rect fullScreen = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(renderer_, &fullScreen);

    drawRect(px, py, POP_W, POP_H, {35, 35, 50, 255});
    drawRectOutline(px, py, POP_W, POP_H, COLOR_TERA, 2);

    int y = py + 15;
    int lx = px + 20;
    int rx = px + POP_W / 2 + 10;
    int lineH = 24;

    // Sprite in top-left (fit to box, preserve aspect ratio)
    constexpr int DETAIL_SPRITE_SIZE = 80;
    SDL_Texture* sprite = getSprite(raid.details.species);
    if (sprite) {
        int texW, texH;
        SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
        float scale = std::min((float)DETAIL_SPRITE_SIZE / texW, (float)DETAIL_SPRITE_SIZE / texH);
        int dstW = (int)(texW * scale);
        int dstH = (int)(texH * scale);
        SDL_Rect dst = {lx + (DETAIL_SPRITE_SIZE - dstW) / 2,
                        y + (DETAIL_SPRITE_SIZE - dstH) / 2, dstW, dstH};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    // Title: species name + form (next to sprite)
    int titleX = lx + DETAIL_SPRITE_SIZE + 12;
    std::string species = getSpeciesName(raid.details.species);
    if (raid.details.form > 0)
        species += " (Form " + std::to_string(raid.details.form) + ")";
    drawText(species, titleX, y + 10, COLOR_TEXT, fontLarge_);

    // Star rating top-right
    std::string stars = getStarString(raid.details.stars);
    drawTextRight(stars, px + POP_W - 20, y + 10, COLOR_SHINY, font_);

    y += DETAIL_SPRITE_SIZE + 8;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 70, 70, 90, 255);
    SDL_RenderDrawLine(renderer_, lx, y, px + POP_W - 20, y);
    y += 10;

    // Left column
    int ly = y;
    drawText("Tera Type:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    drawText(getTypeName(raid.details.teraType), lx + 85, ly, getTypeColor(raid.details.teraType), fontSmall_);
    ly += lineH;

    drawText("Level:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    char lvlBuf[16];
    snprintf(lvlBuf, sizeof(lvlBuf), "%d", raid.details.level);
    drawText(lvlBuf, lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Nature:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    drawText(getNatureName(raid.details.nature), lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Ability:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    drawText(getAbilityName(raid.details.ability), lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Gender:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    const char* genderStr = "---";
    switch (raid.details.gender) {
        case Gender::Male:       genderStr = "Male"; break;
        case Gender::Female:     genderStr = "Female"; break;
        case Gender::Genderless: genderStr = "Genderless"; break;
    }
    drawText(genderStr, lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Shiny:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    if (raid.details.shiny == TeraShiny::Star)
        drawText("Star", lx + 85, ly, COLOR_SHINY, fontSmall_);
    else if (raid.details.shiny == TeraShiny::Square)
        drawText("Square", lx + 85, ly, COLOR_SHINY, fontSmall_);
    else
        drawText("No", lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    // Right column
    int ry = y;
    drawText("IVs:", rx, ry, COLOR_TEXT_DIM, fontSmall_);
    ry += lineH;

    const char* statNames[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
    for (int i = 0; i < 6; i++) {
        char ivLine[32];
        snprintf(ivLine, sizeof(ivLine), "%-4s %2d", statNames[i], raid.details.ivs[i]);
        SDL_Color col = (raid.details.ivs[i] == 31) ? COLOR_SHINY : COLOR_TEXT;
        drawText(ivLine, rx + 10, ry, col, fontSmall_);
        ry += 18;
    }

    ry += 6;
    drawText("Moves:", rx, ry, COLOR_TEXT_DIM, fontSmall_);
    ry += lineH;
    for (int i = 0; i < 4; i++) {
        if (raid.details.moves[i] > 0) {
            drawText(getMoveName(raid.details.moves[i]), rx + 10, ry, COLOR_TEXT, fontSmall_);
            ry += 18;
        }
    }

    // Seed at bottom
    char seedBuf[32];
    snprintf(seedBuf, sizeof(seedBuf), "Seed: %08X", raid.details.seed);
    drawText(seedBuf, lx, py + POP_H - 35, {100, 100, 120, 255}, fontSmall_);

    // EC/PID
    char ecpid[48];
    snprintf(ecpid, sizeof(ecpid), "EC: %08X  PID: %08X", raid.details.EC, raid.details.PID);
    drawText(ecpid, lx, py + POP_H - 55, {100, 100, 120, 255}, fontTiny_);

    // Close hint
    drawTextRight("B: Close", px + POP_W - 20, py + POP_H - 35, COLOR_TEXT_DIM, fontSmall_);
}

void UI::handleRaidViewInput(bool& running) {
    int count = (int)filteredIndices_.size();

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
            if (showDetail_) {
                // Close detail with B button only (SDL_A = Switch B)
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                    showDetail_ = false;
                continue;
            }

            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (count > 0) raidCursor_ = (raidCursor_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (count > 0) raidCursor_ = (raidCursor_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: { // L = prev map tab
                    int m = ((int)currentMap_ + 2) % 3;
                    currentMap_ = (TeraRaidMapParent)m;
                    raidCursor_ = 0; raidScroll_ = 0;
                    rebuildFilteredList();
                    break;
                }
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: { // R = next map tab
                    int m = ((int)currentMap_ + 1) % 3;
                    currentMap_ = (TeraRaidMapParent)m;
                    raidCursor_ = 0; raidScroll_ = 0;
                    rebuildFilteredList();
                    break;
                }
                case SDL_CONTROLLER_BUTTON_B: // Switch A = detail
                    if (count > 0) showDetail_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                    if (!liveMode_)
                        screen_ = AppScreen::GameSelector;
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true; break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false;
                    break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            if (showDetail_) {
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    showDetail_ = false;
                continue;
            }

            switch (event.key.keysym.sym) {
                case SDLK_UP:
                    if (count > 0) raidCursor_ = (raidCursor_ + count - 1) % count;
                    break;
                case SDLK_DOWN:
                    if (count > 0) raidCursor_ = (raidCursor_ + 1) % count;
                    break;
                case SDLK_q: { // L
                    int m = ((int)currentMap_ + 2) % 3;
                    currentMap_ = (TeraRaidMapParent)m;
                    raidCursor_ = 0; raidScroll_ = 0;
                    rebuildFilteredList();
                    break;
                }
                case SDLK_e: { // R
                    int m = ((int)currentMap_ + 1) % 3;
                    currentMap_ = (TeraRaidMapParent)m;
                    raidCursor_ = 0; raidScroll_ = 0;
                    rebuildFilteredList();
                    break;
                }
                case SDLK_a: case SDLK_RETURN:
                    if (count > 0) showDetail_ = true;
                    break;
                case SDLK_MINUS:
                    showAbout_ = true; break;
                case SDLK_b: case SDLK_ESCAPE:
                    if (!liveMode_)
                        screen_ = AppScreen::GameSelector;
                    break;
            }
        }
    }

    // Joystick repeat
    if (!showDetail_ && (stickDirY_ != 0)) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay && count > 0) {
            if (stickDirY_ < 0)
                raidCursor_ = (raidCursor_ + count - 1) % count;
            else
                raidCursor_ = (raidCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

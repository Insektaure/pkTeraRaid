#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- SwSh Den Crawler UI ---

void UI::runSwSh(const std::string& basePath, GameVersion game) {
    basePath_ = basePath;
    liveMode_ = true;
    selectedVersion_ = game;

    showWorking("Loading resources...");

#ifdef __SWITCH__
    std::string dataDir = "romfs:/data/";
    std::string mapDir  = "romfs:/maps/";
#else
    std::string dataDir = "romfs/data/";
    std::string mapDir  = "romfs/maps/";
#endif

    speciesNames_ = TextData::loadLines(dataDir + "species_en.txt");
    typeNames_    = TextData::loadLines(dataDir + "types_en.txt");
    personal_.load(dataDir + "personal_sv");

    // Load SwSh map images
    auto loadMap = [&](const char* name) -> SDL_Texture* {
        std::string path = mapDir + name;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return nullptr;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        return tex;
    };
    if (!mapWildArea_) mapWildArea_ = loadMap("wild_area.png");
    if (!mapIoA_) mapIoA_ = loadMap("isle_of_armor.png");
    if (!mapCT_) mapCT_ = loadMap("crown_tundra.png");

    showWorking("Reading den data from memory...");

    if (!denCrawler_.readLive(game)) {
        showMessageAndWait("Error", "Failed to read den data from game memory.");
        return;
    }

    swshTab_ = 0;
    swshCursor_ = 0;
    swshScroll_ = 0;
    swshShowDetail_ = false;
    swshShowAll_ = false;
    rebuildSwShFilteredList();

    bool running = true;
    while (running) {
        if (showAbout_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                        showAbout_ = false;
                }
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_MINUS ||
                        event.key.keysym.sym == SDLK_b ||
                        event.key.keysym.sym == SDLK_ESCAPE)
                        showAbout_ = false;
                }
            }
            drawSwShViewFrame();
            drawAboutPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        handleSwShViewInput(running);
        drawSwShViewFrame();
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

void UI::rebuildSwShFilteredList() {
    swshFiltered_.clear();
    auto& dens = denCrawler_.dens();

    SwShDenRegion targetRegion;
    switch (swshTab_) {
        case 0: targetRegion = SwShDenRegion::Vanilla; break;
        case 1: targetRegion = SwShDenRegion::IslandOfArmor; break;
        default: targetRegion = SwShDenRegion::CrownTundra; break;
    }

    for (int i = 0; i < (int)dens.size(); i++) {
        if (dens[i].region == targetRegion && (swshShowAll_ || dens[i].isActive))
            swshFiltered_.push_back(i);
    }

    if (swshCursor_ >= (int)swshFiltered_.size())
        swshCursor_ = std::max(0, (int)swshFiltered_.size() - 1);
    if (swshScroll_ > swshCursor_)
        swshScroll_ = swshCursor_;
}

void UI::drawSwShViewFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Title bar
    const char* gameName = gameDisplayNameOf(selectedVersion_);
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

    drawSwShMapPanel();
    drawSwShListPanel();

    if (swshShowDetail_ && !swshFiltered_.empty()) {
        int idx = swshFiltered_[swshCursor_];
        drawSwShDetailPopup(denCrawler_.dens()[idx]);
    }

    std::string status = liveMode_ ? "Live Mode - " : "Save File - ";
    status += gameName;
    if (liveMode_)
        status += "  |  D-Pad:Navigate  A:Detail  X:Toggle  L/R:Map Tab  -:About  +:Quit";
    else
        status += "  |  D-Pad:Navigate  A:Detail  X:Toggle  B:Back  L/R:Map Tab  -:About  +:Quit";
    drawStatusBar(status);
}

void UI::drawSwShMapPanel() {
    drawRect(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W, MAP_PANEL_H, COLOR_PANEL_BG);

    drawSwShMapTabs(MAP_PANEL_X, MAP_PANEL_Y, MAP_PANEL_W);

    int areaX = MAP_PANEL_X + 5;
    int areaY = MAP_PANEL_Y + TAB_H + 5;
    int areaW = MAP_PANEL_W - 10;
    int areaH = MAP_PANEL_H - TAB_H - 10;

    // Select map texture and image dimensions
    SDL_Texture* mapTex = nullptr;
    int imgW = 1, imgH = 1;
    switch (swshTab_) {
        case 0:
            mapTex = mapWildArea_;
            imgW = SwShDenLocations::MAP_IMG_W_WA;
            imgH = SwShDenLocations::MAP_IMG_H_WA;
            break;
        case 1:
            mapTex = mapIoA_;
            imgW = SwShDenLocations::MAP_IMG_W_IOA;
            imgH = SwShDenLocations::MAP_IMG_H_IOA;
            break;
        case 2:
            mapTex = mapCT_;
            imgW = SwShDenLocations::MAP_IMG_W_CT;
            imgH = SwShDenLocations::MAP_IMG_H_CT;
            break;
    }

    // Fit map preserving aspect ratio
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
        drawRect(mapX, mapY, mapW, mapH, {25, 35, 25, 255});
    }

    // Draw den dots
    auto& dens = denCrawler_.dens();

    if (swshFiltered_.empty()) {
        drawTextCentered("No active dens", mapX + mapW / 2, mapY + mapH / 2,
                         COLOR_TEXT_DIM, font_);
        return;
    }

    for (int fi = 0; fi < (int)swshFiltered_.size(); fi++) {
        int idx = swshFiltered_[fi];
        auto& den = dens[idx];

        if (den.denIndex < 0 || den.denIndex >= SwShDenLocations::DEN_COUNT)
            continue;
        auto& loc = SwShDenLocations::DEN_LOCATIONS[den.denIndex];

        // Convert pixel coords to screen coords
        int sx = mapX + (int)((float)loc.mapX / imgW * mapW);
        int sy = mapY + (int)((float)loc.mapY / imgH * mapH);

        // Skip dots outside the map area
        if (sx < mapX || sx > mapX + mapW || sy < mapY || sy > mapY + mapH)
            continue;

        bool isSelected = (fi == swshCursor_);
        int radius = isSelected ? 7 : 5;

        // Color: gold=currently shiny (advance 1), red=active, dimmed for event
        SDL_Color dotColor;
        bool isCurrentlyShiny = (den.shinyType != SwShShinyType::None && den.shinyAdvance == 1);
        if (den.isEvent) {
            dotColor = COLOR_MAP_DOT_6;
        } else if (isCurrentlyShiny) {
            dotColor = COLOR_SHINY;
        } else {
            dotColor = COLOR_MAP_DOT;
        }

        // Rare dens get slightly larger dot
        if (den.isRare && !isSelected)
            radius = 6;

        fillCircle(sx, sy, radius, dotColor);

        if (isSelected) {
            SDL_SetRenderDrawColor(renderer_, COLOR_CURSOR.r, COLOR_CURSOR.g,
                                   COLOR_CURSOR.b, 255);
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
}

void UI::drawSwShMapTabs(int x, int y, int w) {
    constexpr int NUM_TABS = 3;
    int tabW = w / NUM_TABS;

    const char* names[] = {"Wild Area", "Isle of Armor", "Crown Tundra"};

    for (int i = 0; i < NUM_TABS; i++) {
        int tx = x + i * tabW;
        bool active = (i == swshTab_);

        if (active) {
            drawRect(tx, y, tabW, TAB_H, {60, 60, 90, 255});
            drawTextCentered(names[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT, fontSmall_);
        } else {
            drawRect(tx, y, tabW, TAB_H, {35, 35, 50, 255});
            drawTextCentered(names[i], tx + tabW / 2, y + TAB_H / 2, COLOR_TEXT_DIM, fontSmall_);
        }

        if (i < NUM_TABS - 1) {
            SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 255);
            SDL_RenderDrawLine(renderer_, tx + tabW, y + 4, tx + tabW, y + TAB_H - 4);
        }
    }
}

void UI::drawSwShListPanel() {
    drawRect(LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_W, LIST_PANEL_H, COLOR_PANEL_BG);

    auto& dens = denCrawler_.dens();
    int count = (int)swshFiltered_.size();

    // Header
    int headerY = LIST_PANEL_Y + 5;
    const char* regionNames[] = {"Wild Area", "Isle of Armor", "Crown Tundra"};
    char header[64];
    snprintf(header, sizeof(header), "%s: %d  (%s)",
             swshShowAll_ ? "All Dens" : "Active Dens", count,
             regionNames[swshTab_]);
    drawText(header, LIST_PANEL_X + 10, headerY, COLOR_TEXT, fontSmall_);

    int listY = LIST_PANEL_Y + 28;
    int listH = LIST_PANEL_H - 28;

    int visibleRows = std::max(1, listH / 60);
    int rowH = listH / visibleRows;

    if (swshCursor_ < swshScroll_) swshScroll_ = swshCursor_;
    if (swshCursor_ >= swshScroll_ + visibleRows)
        swshScroll_ = swshCursor_ - visibleRows + 1;

    for (int vi = 0; vi < visibleRows && (swshScroll_ + vi) < count; vi++) {
        int fi = swshScroll_ + vi;
        int idx = swshFiltered_[fi];
        int rowY = listY + vi * rowH;
        bool selected = (fi == swshCursor_);

        drawSwShRow(LIST_PANEL_X + 5, rowY, LIST_PANEL_W - 10, dens[idx], selected, rowH);
    }

    // Scrollbar
    if (count > visibleRows) {
        int sbX = LIST_PANEL_X + LIST_PANEL_W - 6;
        int sbH = listH;
        int thumbH = std::max(20, sbH * visibleRows / count);
        int thumbY = listY + (sbH - thumbH) * swshScroll_ / (count - visibleRows);
        drawRect(sbX, listY, 4, sbH, {40, 40, 55, 255});
        drawRect(sbX, thumbY, 4, thumbH, {100, 100, 130, 255});
    }
}

void UI::drawSwShRow(int x, int y, int w, const SwShDenInfo& den, bool selected, int rowH) {
    bool isCurrentlyShiny = (den.shinyType != SwShShinyType::None && den.shinyAdvance == 1);
    int rh = rowH - 2;

    // Background: gold only if currently shiny (advance 1)
    if (isCurrentlyShiny) {
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

    SDL_Color textMain = isCurrentlyShiny ? SDL_Color{30, 25, 10, 255} : COLOR_TEXT;
    SDL_Color textDim  = isCurrentlyShiny ? SDL_Color{70, 60, 20, 255} : COLOR_TEXT_DIM;
    SDL_Color textStar = isCurrentlyShiny ? SDL_Color{100, 60, 0, 255} : COLOR_SHINY;
    SDL_Color textSeed = isCurrentlyShiny ? SDL_Color{90, 75, 25, 255} : SDL_Color{100, 100, 120, 255};

    // Sprite
    int spriteSize = std::min(LIST_SPRITE_SIZE, rh - 4);
    int spriteX = x + 4;
    int spriteY = y + (rh - spriteSize) / 2;

    if (den.species > 0) {
        SDL_Texture* sprite = getSprite(den.species);
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
    }

    int textX = x + spriteSize + 12;

    // Fixed column offsets (relative to textX) matching SV layout
    // Line 1: Stars | Species | Level | Types | Shiny/Tags
    constexpr int COL_STARS    = 0;
    constexpr int COL_SPECIES  = 55;
    constexpr int COL_LEVEL    = 210;
    constexpr int COL_TYPES    = 270;


    int line1Y = y + rh / 2 - 22;
    if (line1Y < y + 2) line1Y = y + 2;

    // Stars (display as 1-5 stars)
    std::string stars = getStarString(den.stars + 1);
    drawText(stars, textX + COL_STARS, line1Y, textStar, font_);

    // Species name
    std::string species;
    SDL_Color speciesColor = textMain;
    if (den.isEvent) {
        species = "Event";
        speciesColor = {180, 130, 220, 255};
    } else if (den.species > 0) {
        species = getSpeciesName(den.species);
    } else {
        species = "Event";
        speciesColor = {180, 130, 220, 255};
    }
    drawText(species, textX + COL_SPECIES, line1Y, speciesColor, font_);

    // Level (derived from star rating)
    if (!den.isEvent && den.species > 0) {
        constexpr int starMaxLevel[] = {20, 30, 40, 50, 60};
        int level = (den.stars <= 4) ? starMaxLevel[den.stars] : 60;
        char lvl[16];
        snprintf(lvl, sizeof(lvl), "Lv.%d", level);
        drawText(lvl, textX + COL_LEVEL, line1Y, textDim, font_);
    }

    // Types (from personal table)
    if (!den.isEvent && den.species > 0) {
        auto info = personal_[den.species];
        uint8_t t1 = info.type1();
        uint8_t t2 = info.type2();
        SDL_Color t1Col = isCurrentlyShiny ? SDL_Color{50, 30, 10, 255} : getTypeColor(t1);
        std::string t1Name = getTypeName(t1);
        drawText(t1Name, textX + COL_TYPES, line1Y, t1Col, font_);
        if (t2 != t1) {
            int tw1, th1;
            TTF_SizeUTF8(font_, t1Name.c_str(), &tw1, &th1);
            drawText(" / ", textX + COL_TYPES + tw1, line1Y, textDim, font_);
            int twSlash, thSlash;
            TTF_SizeUTF8(font_, " / ", &twSlash, &thSlash);
            SDL_Color t2Col = isCurrentlyShiny ? SDL_Color{50, 30, 10, 255} : getTypeColor(t2);
            drawText(getTypeName(t2), textX + COL_TYPES + tw1 + twSlash, line1Y, t2Col, font_);
        }
    }

    // Tags: Rare/Event + Shiny (right-aligned to avoid overlapping types)
    {
        int tagX = x + w - 8;
        if (isCurrentlyShiny) {
            drawTextRight("Shiny!", tagX, line1Y, SDL_Color{120, 40, 0, 255}, font_);
            int tw0, th0;
            TTF_SizeUTF8(font_, "Shiny!", &tw0, &th0);
            tagX -= tw0 + 8;
        }
        if (den.isRare) {
            drawTextRight("Rare", tagX, line1Y, SDL_Color{200, 120, 255, 255}, font_);
        }
        if (den.isEvent) {
            drawTextRight("Event", tagX, line1Y, SDL_Color{100, 200, 255, 255}, font_);
        }
    }

    // Line 2: Location | Shiny info | IVs | Seed
    int line2Y = y + rh / 2 + 4;

    // Location name
    if (den.denIndex >= 0 && den.denIndex < SwShDenLocations::DEN_COUNT) {
        uint8_t locId = SwShDenLocations::DEN_LOCATIONS[den.denIndex].locationId;
        if (locId < SwShDenLocations::LOCATION_COUNT) {
            drawText(SwShDenLocations::LOCATION_NAMES[locId], textX, line2Y, textDim, fontSmall_);
        }
    }

    // Shiny info
    if (den.shinyType != SwShShinyType::None) {
        char shinyBuf[32];
        const char* shinyStr = (den.shinyType == SwShShinyType::Square) ? "Sq" : "Star";
        snprintf(shinyBuf, sizeof(shinyBuf), "%s in %u", shinyStr, den.shinyAdvance);
        drawText(shinyBuf, textX + 180, line2Y, textStar, fontSmall_);
    }

    // Flawless IVs
    if (den.flawlessIVs > 0 && !den.isEvent) {
        char ivBuf[16];
        snprintf(ivBuf, sizeof(ivBuf), "%dIV", den.flawlessIVs);
        drawText(ivBuf, textX + 290, line2Y, textDim, fontSmall_);
    }

    // Seed (right-aligned)
    char seedBuf[24];
    snprintf(seedBuf, sizeof(seedBuf), "%016lX", den.seed);
    drawTextRight(seedBuf, x + w - 8, line2Y, textSeed, fontSmall_);
}

void UI::drawSwShDetailPopup(const SwShDenInfo& den) {
    constexpr int POP_W = 580, POP_H = 430;
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
    int rx = px + 320;
    int lineH = 24;

    // Sprite
    constexpr int DETAIL_SPRITE_SIZE = 80;
    if (den.species > 0) {
        SDL_Texture* sprite = getSprite(den.species);
        if (sprite) {
            int texW, texH;
            SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
            float scale = std::min((float)DETAIL_SPRITE_SIZE / texW,
                                   (float)DETAIL_SPRITE_SIZE / texH);
            int dstW = (int)(texW * scale);
            int dstH = (int)(texH * scale);
            SDL_Rect dst = {lx + (DETAIL_SPRITE_SIZE - dstW) / 2,
                            y + (DETAIL_SPRITE_SIZE - dstH) / 2, dstW, dstH};
            SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
        }
    }

    // Species name next to sprite
    int titleX = lx + DETAIL_SPRITE_SIZE + 12;
    std::string species;
    SDL_Color speciesColor = COLOR_TEXT;
    if (den.isEvent || den.species == 0) {
        species = "Event Den";
        speciesColor = {180, 130, 220, 255};
    } else {
        species = getSpeciesName(den.species);
    }
    drawText(species, titleX, y + 10, speciesColor, fontLarge_);

    // Stars top-right
    std::string stars = getStarString(den.stars + 1);
    drawTextRight(stars, px + POP_W - 20, y + 10, COLOR_SHINY, font_);

    y += DETAIL_SPRITE_SIZE + 8;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 70, 70, 90, 255);
    SDL_RenderDrawLine(renderer_, lx, y, px + POP_W - 20, y);
    y += 10;

    // Left column
    int ly = y;

    drawText("Location:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    if (den.denIndex >= 0 && den.denIndex < SwShDenLocations::DEN_COUNT) {
        uint8_t locId = SwShDenLocations::DEN_LOCATIONS[den.denIndex].locationId;
        if (locId < SwShDenLocations::LOCATION_COUNT)
            drawText(SwShDenLocations::LOCATION_NAMES[locId], lx + 85, ly, COLOR_TEXT, fontSmall_);
    }
    ly += lineH;

    if (!den.isEvent && den.species > 0) {
        drawText("Level:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
        constexpr int starMaxLevel[] = {20, 30, 40, 50, 60};
        int level = (den.stars <= 4) ? starMaxLevel[den.stars] : 60;
        char lvlBuf[16];
        snprintf(lvlBuf, sizeof(lvlBuf), "%d", level);
        drawText(lvlBuf, lx + 85, ly, COLOR_TEXT, fontSmall_);
        ly += lineH;

        drawText("Type:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
        auto info = personal_[den.species];
        uint8_t t1 = info.type1();
        uint8_t t2 = info.type2();
        std::string t1Name = getTypeName(t1);
        drawText(t1Name, lx + 85, ly, getTypeColor(t1), fontSmall_);
        if (t2 != t1) {
            int tw1, th1;
            TTF_SizeUTF8(fontSmall_, t1Name.c_str(), &tw1, &th1);
            drawText(" / ", lx + 85 + tw1, ly, COLOR_TEXT_DIM, fontSmall_);
            int twSlash, thSlash;
            TTF_SizeUTF8(fontSmall_, " / ", &twSlash, &thSlash);
            drawText(getTypeName(t2), lx + 85 + tw1 + twSlash, ly, getTypeColor(t2), fontSmall_);
        }
        ly += lineH;
    }

    drawText("Beam:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    const char* beamStr = den.isEvent ? "Event" : (den.isRare ? "Rare" : "Normal");
    SDL_Color beamCol = den.isEvent ? COLOR_TERA
                      : (den.isRare ? SDL_Color{200, 120, 255, 255} : COLOR_TEXT);
    drawText(beamStr, lx + 85, ly, beamCol, fontSmall_);
    ly += lineH;

    drawText("Den #:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    char denBuf[16];
    snprintf(denBuf, sizeof(denBuf), "%d", den.denIndex);
    drawText(denBuf, lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Seed:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    char seedBuf[24];
    snprintf(seedBuf, sizeof(seedBuf), "%016lX", den.seed);
    drawText(seedBuf, lx + 85, ly, COLOR_TEXT, fontSmall_);
    ly += lineH;

    drawText("Shiny:", lx, ly, COLOR_TEXT_DIM, fontSmall_);
    if (den.shinyType == SwShShinyType::Square) {
        char buf[64];
        if (den.shinyAdvance == 1)
            snprintf(buf, sizeof(buf), "Square (current)");
        else
            snprintf(buf, sizeof(buf), "Square in %u (%u skips)", den.shinyAdvance, den.shinyAdvance - 1);
        drawText(buf, lx + 85, ly, COLOR_SHINY, fontSmall_);
    } else if (den.shinyType == SwShShinyType::Star) {
        char buf[64];
        if (den.shinyAdvance == 1)
            snprintf(buf, sizeof(buf), "Star (current)");
        else
            snprintf(buf, sizeof(buf), "Star in %u (%u skips)", den.shinyAdvance, den.shinyAdvance - 1);
        drawText(buf, lx + 85, ly, COLOR_SHINY, fontSmall_);
    } else {
        drawText("No nearby shiny", lx + 85, ly, COLOR_TEXT, fontSmall_);
    }
    ly += lineH;

    // Right column — IVs only
    int ry = y;

    if (!den.isEvent && den.species > 0) {
        drawText("IVs:", rx, ry, COLOR_TEXT_DIM, fontSmall_);
        ry += lineH;

        const char* statNames[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
        for (int i = 0; i < 6; i++) {
            char ivLine[32];
            snprintf(ivLine, sizeof(ivLine), "%-4s %2d", statNames[i], den.ivs[i]);
            SDL_Color col = (den.ivs[i] == 31) ? COLOR_SHINY :
                            (den.ivs[i] == 0)  ? COLOR_RED   : COLOR_TEXT;
            drawText(ivLine, rx + 10, ry, col, fontSmall_);
            ry += 18;
        }
    }

    // Close hint
    drawTextRight("B: Close", px + POP_W - 20, py + POP_H - 35, COLOR_TEXT_DIM, fontSmall_);
}

void UI::handleSwShViewInput(bool& running) {
    int count = (int)swshFiltered_.size();

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
            if (swshShowDetail_) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                    swshShowDetail_ = false;
                continue;
            }

            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (count > 0) swshCursor_ = (swshCursor_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (count > 0) swshCursor_ = (swshCursor_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    swshTab_ = (swshTab_ + 2) % 3;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    swshTab_ = (swshTab_ + 1) % 3;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = detail
                    if (count > 0) swshShowDetail_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X = toggle active/all
                    swshShowAll_ = !swshShowAll_;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back (save mode only)
                    if (!liveMode_) running = false;
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    showAbout_ = true; break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false; break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            if (swshShowDetail_) {
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    swshShowDetail_ = false;
                continue;
            }

            switch (event.key.keysym.sym) {
                case SDLK_UP:
                    if (count > 0) swshCursor_ = (swshCursor_ + count - 1) % count;
                    break;
                case SDLK_DOWN:
                    if (count > 0) swshCursor_ = (swshCursor_ + 1) % count;
                    break;
                case SDLK_q:
                    swshTab_ = (swshTab_ + 2) % 3;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDLK_e:
                    swshTab_ = (swshTab_ + 1) % 3;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDLK_x:
                    swshShowAll_ = !swshShowAll_;
                    swshCursor_ = 0; swshScroll_ = 0;
                    rebuildSwShFilteredList();
                    break;
                case SDLK_a: case SDLK_RETURN:
                    if (count > 0) swshShowDetail_ = true;
                    break;
                case SDLK_MINUS:
                    showAbout_ = true;
                    break;
                case SDLK_b:
                    if (!liveMode_) running = false;
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
            }
        }
    }

    // Joystick repeat
    if (!swshShowDetail_ && (stickDirY_ != 0)) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay && count > 0) {
            if (stickDirY_ < 0)
                swshCursor_ = (swshCursor_ + count - 1) % count;
            else
                swshCursor_ = (swshCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

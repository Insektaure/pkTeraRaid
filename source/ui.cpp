#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- Init / Shutdown ---

bool UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
        return false;

    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow("pkTeraRaid",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window_) {
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_DestroyWindow(window_);
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

#ifdef __SWITCH__
    PlFontData fontData;
    plInitialize(PlServiceType_System);
    plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    SDL_RWops* rw = SDL_RWFromMem(fontData.address, fontData.size);
    font_      = TTF_OpenFontRW(rw, 0, 18);
    fontSmall_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 0, 14);
    fontLarge_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 0, 28);
    fontTiny_  = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 0, 11);
#else
    font_      = TTF_OpenFont("romfs/fonts/default.ttf", 18);
    fontSmall_ = TTF_OpenFont("romfs/fonts/default.ttf", 14);
    fontLarge_ = TTF_OpenFont("romfs/fonts/default.ttf", 28);
    fontTiny_  = TTF_OpenFont("romfs/fonts/default.ttf", 11);
#endif

    if (!font_) font_ = TTF_OpenFont("romfs:/fonts/default.ttf", 18);
    if (!fontSmall_) fontSmall_ = TTF_OpenFont("romfs:/fonts/default.ttf", 14);
    if (!fontLarge_) fontLarge_ = TTF_OpenFont("romfs:/fonts/default.ttf", 28);
    if (!fontTiny_) fontTiny_ = TTF_OpenFont("romfs:/fonts/default.ttf", 11);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad_ = SDL_GameControllerOpen(i);
            break;
        }
    }

    return true;
}

void UI::shutdown() {
    freeSprites();
    freeGameIcons();
    account_.freeTextures();
    if (mapPaldea_) SDL_DestroyTexture(mapPaldea_);
    if (mapKitakami_) SDL_DestroyTexture(mapKitakami_);
    if (mapBlueberry_) SDL_DestroyTexture(mapBlueberry_);
    if (mapWildArea_) SDL_DestroyTexture(mapWildArea_);
    if (mapIoA_) SDL_DestroyTexture(mapIoA_);
    if (mapCT_) SDL_DestroyTexture(mapCT_);
    if (fontTiny_) TTF_CloseFont(fontTiny_);
    if (fontLarge_) TTF_CloseFont(fontLarge_);
    if (fontSmall_) TTF_CloseFont(fontSmall_);
    if (font_) TTF_CloseFont(font_);
    if (pad_) SDL_GameControllerClose(pad_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
#ifdef __SWITCH__
    plExit();
#endif
}

// --- Splash ---

void UI::showSplash() {
    if (!renderer_) return;

#ifdef __SWITCH__
    const char* splashPath = "romfs:/splash.png";
#else
    const char* splashPath = "romfs/splash.png";
#endif

    SDL_Surface* surf = IMG_Load(splashPath);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int texW, texH;
    SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);

    // Scale to fit screen while preserving aspect ratio
    float scale = std::min(static_cast<float>(SCREEN_W) / texW,
                           static_cast<float>(SCREEN_H) / texH);
    int dstW = static_cast<int>(texW * scale);
    int dstH = static_cast<int>(texH * scale);
    SDL_Rect dst = {(SCREEN_W - dstW) / 2, (SCREEN_H - dstH) / 2, dstW, dstH};

    // Hold splash for ~2.5 seconds
    Uint32 holdMs = 2500;
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < holdMs) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_DestroyTexture(tex);
                return;
            }
        }
        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    // Fade out over ~0.5 seconds
    Uint32 fadeMs = 500;
    start = SDL_GetTicks();
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_DestroyTexture(tex);
                return;
            }
        }
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed >= fadeMs)
            break;
        int alpha = 255 - static_cast<int>(255 * elapsed / fadeMs);
        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer_);
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(alpha));
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(tex);
}

void UI::showMessageAndWait(const std::string& title, const std::string& body) {
    if (!renderer_) return;
    bool waiting = true;
    while (waiting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) waiting = false;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
                    event.cbutton.button == SDL_CONTROLLER_BUTTON_B)
                    waiting = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_RETURN)
                    waiting = false;
            }
        }
        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer_);
        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, COLOR_RED, fontLarge_);

        // Render body with newline support (centered)
        TTF_SetFontWrappedAlign(font_, TTF_WRAPPED_ALIGN_CENTER);
        SDL_Surface* bodySurf = TTF_RenderUTF8_Blended_Wrapped(font_, body.c_str(), COLOR_TEXT_DIM, 0);
        if (bodySurf) {
            SDL_Texture* bodyTex = SDL_CreateTextureFromSurface(renderer_, bodySurf);
            SDL_Rect bodyDst = {(SCREEN_W - bodySurf->w) / 2, SCREEN_H / 2 + 15 - bodySurf->h / 2,
                                bodySurf->w, bodySurf->h};
            SDL_RenderCopy(renderer_, bodyTex, nullptr, &bodyDst);
            SDL_DestroyTexture(bodyTex);
            SDL_FreeSurface(bodySurf);
        }

        drawTextCentered("Press B to dismiss", SCREEN_W / 2, SCREEN_H / 2 + 65, COLOR_TEXT_DIM, fontSmall_);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

void UI::showWorking(const std::string& msg) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    constexpr int POP_W = 400, POP_H = 100;
    int popX = (SCREEN_W - POP_W) / 2, popY = (SCREEN_H - POP_H) / 2;
    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_TEXT_DIM, 2);
    drawTextCentered(msg, SCREEN_W / 2, SCREEN_H / 2, COLOR_TEXT, font_);
    SDL_RenderPresent(renderer_);
}

// --- Run ---

void UI::run(const std::string& basePath) {
    basePath_ = basePath;

#ifdef __SWITCH__
    showWorking("Loading profiles...");
    if (account_.init() && account_.loadProfiles(renderer_)) {
        screen_ = AppScreen::ProfileSelector;
    } else {
        // No profiles — show both games
        availableGames_ = {GameVersion::Scarlet, GameVersion::Violet};
        screen_ = AppScreen::GameSelector;
        showWorking("Loading...");
        loadGameIcons();
    }
#else
    availableGames_ = {GameVersion::Scarlet, GameVersion::Violet};
    screen_ = AppScreen::GameSelector;
    loadGameIcons();
#endif

    bool running = true;
    while (running) {
        // About popup intercepts input from any screen
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
            // Draw underlying screen, then about on top
            if (screen_ == AppScreen::ProfileSelector) drawProfileSelectorFrame();
            else if (screen_ == AppScreen::GameSelector) drawGameSelectorFrame();
            else drawRaidViewFrame();
            drawAboutPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        AppScreen screenBefore = screen_;
        if (screen_ == AppScreen::ProfileSelector) {
            handleProfileSelectorInput(running);
            if (screen_ == screenBefore) drawProfileSelectorFrame();
        } else if (screen_ == AppScreen::GameSelector) {
            handleGameSelectorInput(running);
            if (screen_ == screenBefore) drawGameSelectorFrame();
        } else if (screen_ == AppScreen::RaidView) {
            handleRaidViewInput(running);
            if (screen_ == screenBefore) drawRaidViewFrame();
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

void UI::runLive(const std::string& basePath, GameVersion game) {
    basePath_ = basePath;
    liveMode_ = true;
    selectedVersion_ = game;

    showWorking("Loading resources...");

    // Load text data
#ifdef __SWITCH__
    std::string dataDir = "romfs:/data/";
    std::string mapDir  = "romfs:/maps/";
#else
    std::string dataDir = "romfs/data/";
    std::string mapDir  = "romfs/maps/";
#endif

    speciesNames_ = TextData::loadLines(dataDir + "species_en.txt");
    moveNames_    = TextData::loadLines(dataDir + "moves_en.txt");
    natureNames_  = TextData::loadLines(dataDir + "natures_en.txt");
    abilityNames_ = TextData::loadLines(dataDir + "abilities_en.txt");
    typeNames_    = TextData::loadLines(dataDir + "types_en.txt");
    itemNames_    = TextData::loadLines(dataDir + "items_en.txt");

    if (!reader_.loadResources(dataDir)) {
        showMessageAndWait("Error", "Failed to load encounter data.");
        return;
    }

    // Load map images
    auto loadMap = [&](const char* name) -> SDL_Texture* {
        std::string path = mapDir + name;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return nullptr;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        return tex;
    };
    if (!mapPaldea_) mapPaldea_ = loadMap("paldea.jpg");
    if (!mapKitakami_) mapKitakami_ = loadMap("kitakami.jpg");
    if (!mapBlueberry_) mapBlueberry_ = loadMap("blueberry.jpg");

    // Read raids from live game memory
    showWorking("Reading raids from memory...");

    if (!reader_.readLive(game)) {
        showMessageAndWait("Error", "Failed to read raid data from game memory.");
        return;
    }

    // Start on Paldea
    currentMap_ = TeraRaidMapParent::Paldea;
    raidCursor_ = 0;
    raidScroll_ = 0;
    showDetail_ = false;
    svFilterPreset_ = SvFilterPreset::All;
    svFilterShiny_ = false;
    rebuildFilteredList();

    screen_ = AppScreen::RaidView;

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
            drawRaidViewFrame();
            drawAboutPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        handleRaidViewInput(running);
        drawRaidViewFrame();
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

// --- Profile Selector ---

void UI::drawProfileSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);
    drawTextCentered("Select Profile", SCREEN_W / 2, 40, COLOR_TEXT, font_);

    const auto& profiles = account_.profiles();
    int count = (int)profiles.size();

    constexpr int CARD_W = 160, CARD_H = 200, CARD_GAP = 20, ICON_SIZE = 128;
    int totalW = count * CARD_W + (count - 1) * CARD_GAP;
    int startX = (SCREEN_W - totalW) / 2;
    int startY = (SCREEN_H - CARD_H) / 2;

    for (int i = 0; i < count; i++) {
        int cx = startX + i * (CARD_W + CARD_GAP);
        int cy = startY;

        if (i == profileSelCursor_) {
            drawRect(cx, cy, CARD_W, CARD_H, {60, 60, 80, 255});
            drawRectOutline(cx, cy, CARD_W, CARD_H, COLOR_CURSOR, 3);
        } else {
            drawRect(cx, cy, CARD_W, CARD_H, COLOR_PANEL_BG);
        }

        int iconX = cx + (CARD_W - ICON_SIZE) / 2;
        int iconY = cy + 10;
        if (profiles[i].iconTexture) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, profiles[i].iconTexture, nullptr, &dst);
        } else {
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, {80, 80, 120, 255});
            if (!profiles[i].nickname.empty()) {
                std::string initial(1, profiles[i].nickname[0]);
                drawTextCentered(initial, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2, COLOR_TEXT, font_);
            }
        }

        std::string name = profiles[i].nickname;
        if (name.length() > 14) name = name.substr(0, 13) + ".";
        drawTextCentered(name, cx + CARD_W / 2, cy + ICON_SIZE + 24, COLOR_TEXT, fontSmall_);
    }

    drawStatusBar("A:Select  -:About  +:Quit");
}

void UI::handleProfileSelectorInput(bool& running) {
    int count = account_.profileCount();
    if (count == 0) return;

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
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count; break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A
                    selectProfile(profileSelCursor_); break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true; break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false; break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count; break;
                case SDLK_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count; break;
                case SDLK_a: case SDLK_RETURN:
                    selectProfile(profileSelCursor_); break;
                case SDLK_MINUS:
                    showAbout_ = true; break;
                case SDLK_ESCAPE:
                    running = false; break;
            }
        }
    }

    if (stickDirX_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirX_ < 0)
                profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
            else
                profileSelCursor_ = (profileSelCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

void UI::selectProfile(int index) {
    selectedProfile_ = index;

    // Only show games that have save data for this profile
    availableGames_.clear();
    constexpr GameVersion allGames[] = {
        GameVersion::Sword, GameVersion::Shield,
        GameVersion::Scarlet, GameVersion::Violet
    };
    for (auto g : allGames) {
        if (account_.hasSaveData(index, g))
            availableGames_.push_back(g);
    }

    if (availableGames_.empty()) {
        showMessageAndWait("No Save Data",
            "No Pokemon save data found for this profile.");
        return;
    }

    gameSelCursor_ = 0;
    showWorking("Loading...");
    loadGameIcons();
    screen_ = AppScreen::GameSelector;
}

// --- Game Icons ---

void UI::loadGameIcons() {
    freeGameIcons();
#ifdef __SWITCH__
    std::string cacheDir = basePath_ + "cache/";
    mkdir(cacheDir.c_str(), 0755);

    constexpr GameVersion games[] = {
        GameVersion::Sword, GameVersion::Shield,
        GameVersion::Scarlet, GameVersion::Violet
    };

    // Try loading from cache first
    bool needSystem = false;
    for (auto game : games) {
        char hexId[32];
        std::snprintf(hexId, sizeof(hexId), "%016lX", titleIdOf(game));
        std::string cachePath = cacheDir + hexId + ".jpg";

        SDL_Surface* surf = IMG_Load(cachePath.c_str());
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex)
                gameIconCache_[game] = tex;
            continue;
        }
        needSystem = true;
    }

    // Fetch uncached icons from system
    if (needSystem) {
        nsInitialize();
        for (auto game : games) {
            if (gameIconCache_.count(game))
                continue;

            NsApplicationControlData ctrlData;
            std::memset(&ctrlData, 0, sizeof(ctrlData));
            uint64_t controlSize = 0;
            Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                            titleIdOf(game), &ctrlData, sizeof(ctrlData), &controlSize);
            if (R_FAILED(rc) || controlSize <= sizeof(NacpStruct))
                continue;

            size_t iconSize = controlSize - sizeof(NacpStruct);

            // Save JPEG to cache
            char hexId[32];
            std::snprintf(hexId, sizeof(hexId), "%016lX", titleIdOf(game));
            std::string cachePath = cacheDir + hexId + ".jpg";
            FILE* f = std::fopen(cachePath.c_str(), "wb");
            if (f) {
                std::fwrite(ctrlData.icon, 1, iconSize, f);
                std::fclose(f);
            }

            // Decode and create texture
            SDL_RWops* rw = SDL_RWFromMem(ctrlData.icon, iconSize);
            if (!rw) continue;
            SDL_Surface* surf = IMG_Load_RW(rw, 1);
            if (!surf) continue;
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex) gameIconCache_[game] = tex;
        }
        nsExit();
    }
#endif
}

void UI::freeGameIcons() {
    for (auto& [g, tex] : gameIconCache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    gameIconCache_.clear();
}

// --- Game Selector ---

void UI::drawGameSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);
    drawTextCentered("Select Game Version", SCREEN_W / 2, 60, COLOR_TEXT, fontLarge_);

    int numGames = (int)availableGames_.size();
    constexpr int CARD_W = 200, CARD_H = 250, CARD_GAP = 60, ICON_SIZE = 160;
    int totalW = numGames * CARD_W + (numGames - 1) * CARD_GAP;
    int startX = (SCREEN_W - totalW) / 2;
    int startY = (SCREEN_H - CARD_H) / 2;

    for (int i = 0; i < numGames; i++) {
        int cx = startX + i * (CARD_W + CARD_GAP);
        int cy = startY;

        if (i == gameSelCursor_) {
            drawRect(cx, cy, CARD_W, CARD_H, {60, 60, 80, 255});
            drawRectOutline(cx, cy, CARD_W, CARD_H, COLOR_CURSOR, 3);
        } else {
            drawRect(cx, cy, CARD_W, CARD_H, COLOR_PANEL_BG);
        }

        int iconX = cx + (CARD_W - ICON_SIZE) / 2;
        int iconY = cy + 12;

        auto it = gameIconCache_.find(availableGames_[i]);
        if (it != gameIconCache_.end() && it->second) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, it->second, nullptr, &dst);
        } else {
            // Colored placeholder
            SDL_Color bg;
            const char* letter;
            switch (availableGames_[i]) {
                case GameVersion::Scarlet: bg = {180, 50, 50, 255}; letter = "S"; break;
                case GameVersion::Violet:  bg = {80, 50, 180, 255}; letter = "V"; break;
                case GameVersion::Sword:   bg = {50, 100, 180, 255}; letter = "Sw"; break;
                case GameVersion::Shield:  bg = {180, 50, 100, 255}; letter = "Sh"; break;
            }
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, bg);
            drawTextCentered(letter,
                iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2, COLOR_TEXT, fontLarge_);
        }

        const char* name = gameDisplayNameOf(availableGames_[i]);
        // Strip "Pokemon " prefix for card label
        if (std::strncmp(name, "Pokemon ", 8) == 0) name += 8;
        drawTextCentered(name, cx + CARD_W / 2, cy + ICON_SIZE + 40, COLOR_TEXT, font_);
    }

    if (selectedProfile_ >= 0) {
        drawStatusBar("A:Select  B:Back  -:About  +:Quit");
    } else {
        drawStatusBar("A:Select  -:About  +:Quit");
    }
}

void UI::handleGameSelectorInput(bool& running) {
    int numGames = (int)availableGames_.size();
    if (numGames == 0) return;

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
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    gameSelCursor_ = (gameSelCursor_ + numGames - 1) % numGames; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    gameSelCursor_ = (gameSelCursor_ + 1) % numGames; break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A
                    selectGame(availableGames_[gameSelCursor_]);
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                    if (selectedProfile_ >= 0) {
                        account_.unmountSave();
                        screen_ = AppScreen::ProfileSelector;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true; break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false; break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    gameSelCursor_ = (gameSelCursor_ + numGames - 1) % numGames; break;
                case SDLK_RIGHT:
                    gameSelCursor_ = (gameSelCursor_ + 1) % numGames; break;
                case SDLK_a: case SDLK_RETURN:
                    selectGame(availableGames_[gameSelCursor_]);
                    break;
                case SDLK_MINUS:
                    showAbout_ = true; break;
                case SDLK_b: case SDLK_ESCAPE:
                    if (selectedProfile_ >= 0)
                        screen_ = AppScreen::ProfileSelector;
                    else
                        running = false;
                    break;
            }
        }
    }

    if (stickDirX_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirX_ < 0)
                gameSelCursor_ = (gameSelCursor_ + numGames - 1) % numGames;
            else
                gameSelCursor_ = (gameSelCursor_ + 1) % numGames;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

void UI::selectGame(GameVersion game) {
    selectedVersion_ = game;

    showWorking("Loading resources...");

#ifdef __SWITCH__
    std::string dataDir = "romfs:/data/";
    std::string mapDir  = "romfs:/maps/";
#else
    std::string dataDir = "romfs/data/";
    std::string mapDir  = "romfs/maps/";
#endif

    if (isSwSh(game)) {
        // --- Sword / Shield save file path ---
        speciesNames_ = TextData::loadLines(dataDir + "species_en.txt");
        typeNames_    = TextData::loadLines(dataDir + "types_en.txt");
        personal_.load(dataDir + "personal_sv");

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

        showWorking("Reading save file...");

        std::string savePath;
#ifdef __SWITCH__
        if (selectedProfile_ >= 0) {
            savePath = account_.mountSave(selectedProfile_, game);
            if (savePath.empty()) {
                showMessageAndWait("Error", "Failed to mount save data.");
                return;
            }
            savePath += saveFileNameOf(game);
        }
#else
        savePath = basePath_ + "main";
#endif

        if (!denCrawler_.readSave(savePath, game)) {
            showMessageAndWait("Error", "Failed to read den data from save file.");
#ifdef __SWITCH__
            account_.unmountSave();
#endif
            return;
        }

#ifdef __SWITCH__
        account_.unmountSave();
#endif

        liveMode_ = false;
        swshTab_ = 0;
        swshCursor_ = 0;
        swshScroll_ = 0;
        swshShowDetail_ = false;
        swshShowAll_ = false;
        rebuildSwShFilteredList();

        // Enter SwSh view loop (reuses the same draw/input as live mode)
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

        // Return to game selector
        return;
    }

    // --- Scarlet / Violet save file path ---

    // Load text data
    speciesNames_ = TextData::loadLines(dataDir + "species_en.txt");
    moveNames_    = TextData::loadLines(dataDir + "moves_en.txt");
    natureNames_  = TextData::loadLines(dataDir + "natures_en.txt");
    abilityNames_ = TextData::loadLines(dataDir + "abilities_en.txt");
    typeNames_    = TextData::loadLines(dataDir + "types_en.txt");
    itemNames_    = TextData::loadLines(dataDir + "items_en.txt");

    if (!reader_.loadResources(dataDir)) {
        showMessageAndWait("Error", "Failed to load encounter data.");
        return;
    }

    // Load map images
    auto loadMap = [&](const char* name) -> SDL_Texture* {
        std::string path = mapDir + name;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return nullptr;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        return tex;
    };
    if (!mapPaldea_) mapPaldea_ = loadMap("paldea.jpg");
    if (!mapKitakami_) mapKitakami_ = loadMap("kitakami.jpg");
    if (!mapBlueberry_) mapBlueberry_ = loadMap("blueberry.jpg");

    // Mount + read save
    showWorking("Reading save file...");

    std::string savePath;
#ifdef __SWITCH__
    if (selectedProfile_ >= 0) {
        savePath = account_.mountSave(selectedProfile_, game);
        if (savePath.empty()) {
            showMessageAndWait("Error", "Failed to mount save data.");
            return;
        }
        savePath += saveFileNameOf(game);
    }
#else
    savePath = basePath_ + "main";
#endif

    if (!reader_.readSave(savePath, game)) {
        showMessageAndWait("Error", "Failed to read raid data from save file.");
#ifdef __SWITCH__
        account_.unmountSave();
#endif
        return;
    }

#ifdef __SWITCH__
    account_.unmountSave();
#endif

    // Start on Paldea
    currentMap_ = TeraRaidMapParent::Paldea;
    raidCursor_ = 0;
    raidScroll_ = 0;
    showDetail_ = false;
    svFilterPreset_ = SvFilterPreset::All;
    svFilterShiny_ = false;
    rebuildFilteredList();

    screen_ = AppScreen::RaidView;
}

// --- About ---

void UI::drawAboutPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 187});

    constexpr int POP_W = 700;
    constexpr int POP_H = 440;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    drawRect(px, py, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(px, py, POP_W, POP_H, {0x30, 0x30, 0x55, 255}, 2);

    int cx = px + POP_W / 2;
    int y = py + 25;

    // Title
    drawTextCentered(APP_TITLE, cx, y, COLOR_SHINY, fontLarge_);
    y += 38;

    // Version / author
    drawTextCentered("v" APP_VERSION " - Developed by " APP_AUTHOR, cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 22;
    drawTextCentered("github.com/Insektaure", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 0x30, 0x30, 0x55, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Description
    drawTextCentered("Raid viewer for Pokemon Scarlet, Violet, Sword & Shield", cx, y, COLOR_TEXT, font_);
    y += 28;
    drawTextCentered("Reads save data or live game memory from the console.", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 20;
    drawTextCentered("SV: species, IVs, nature, ability, tera type, moves, shiny status.", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 20;
    drawTextCentered("SwSh: den map, species, shiny prediction, beam type, seed.", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 28;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 0x30, 0x30, 0x55, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 18;

    // Credits
    drawTextCentered("Based on PKHeX by kwsch & Tera-Finder by Manu098vm.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 18;
    drawTextCentered("SwSh den data from CaptureSight & PKHeX Raid Plugin.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 18;
    drawTextCentered("Map formulas from RaidCrawler by LegoFigure11.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 18;
    drawTextCentered("Save access based on JKSV by J-D-K.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 30;

    // Controls
    drawTextCentered("Controls", cx, y, COLOR_TERA, font_);
    y += 26;
    drawTextCentered("D-Pad: Navigate  A: Details  B: Back  X: Filter  Y: Shiny  L/R: Map  +: Quit", cx, y, COLOR_TEXT_DIM, fontSmall_);

    // Footer
    drawTextCentered("Press - or B to close", cx, py + POP_H - 22, COLOR_TEXT_DIM, fontSmall_);
}

// --- Rendering helpers ---

void UI::drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void UI::drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    int tw, th;
    TTF_SizeUTF8(f, text.c_str(), &tw, &th);
    drawText(text, cx - tw / 2, cy - th / 2, color, f);
}

void UI::drawTextRight(const std::string& text, int rx, int y, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    int tw, th;
    TTF_SizeUTF8(f, text.c_str(), &tw, &th);
    drawText(text, rx - tw, y, color, f);
}

void UI::drawRect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void UI::drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect r = {x + t, y + t, w - 2 * t, h - 2 * t};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void UI::drawStatusBar(const std::string& msg) {
    drawRect(0, SCREEN_H - 35, SCREEN_W, 35, {20, 20, 30, 255});
    drawText(msg, 15, SCREEN_H - 30, COLOR_STATUS, fontSmall_);
}

void UI::fillCircle(int cx, int cy, int r, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)std::sqrt(r * r - dy * dy);
        SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void UI::updateStick(int16_t axisX, int16_t axisY) {
    int newDirX = 0, newDirY = 0;
    if (axisX < -STICK_DEADZONE) newDirX = -1;
    else if (axisX > STICK_DEADZONE) newDirX = 1;
    if (axisY < -STICK_DEADZONE) newDirY = -1;
    else if (axisY > STICK_DEADZONE) newDirY = 1;

    if (newDirX != stickDirX_ || newDirY != stickDirY_) {
        stickDirX_ = newDirX;
        stickDirY_ = newDirY;
        stickMoved_ = false;
        stickMoveTime_ = 0;
    }
}

// --- Sprites ---

SDL_Texture* UI::getSprite(uint16_t species) {
    auto it = spriteCache_.find(species);
    if (it != spriteCache_.end())
        return it->second;

    char filename[64];
    std::snprintf(filename, sizeof(filename), "%03d.png", species);

    std::string path;
#ifdef __SWITCH__
    path = std::string("romfs:/sprites/") + filename;
#else
    path = std::string("romfs/sprites/") + filename;
#endif

    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        spriteCache_[species] = nullptr;
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    spriteCache_[species] = tex;
    return tex;
}

void UI::freeSprites() {
    for (auto& [id, tex] : spriteCache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    spriteCache_.clear();
}

// --- Name lookups ---

std::string UI::getSpeciesName(uint16_t species) const {
    if (species < speciesNames_.size())
        return speciesNames_[species];
    char buf[16];
    snprintf(buf, sizeof(buf), "#%u", species);
    return buf;
}

std::string UI::getMoveName(uint16_t move) const {
    if (move < moveNames_.size())
        return moveNames_[move];
    char buf[16];
    snprintf(buf, sizeof(buf), "Move %u", move);
    return buf;
}

std::string UI::getNatureName(uint8_t nature) const {
    if (nature < natureNames_.size())
        return natureNames_[nature];
    char buf[16];
    snprintf(buf, sizeof(buf), "Nature %u", nature);
    return buf;
}

std::string UI::getAbilityName(int ability) const {
    if (ability >= 0 && ability < (int)abilityNames_.size())
        return abilityNames_[ability];
    char buf[16];
    snprintf(buf, sizeof(buf), "Ability %d", ability);
    return buf;
}

std::string UI::getTypeName(uint8_t type) const {
    if (type < typeNames_.size())
        return typeNames_[type];
    char buf[16];
    snprintf(buf, sizeof(buf), "Type %u", type);
    return buf;
}

std::string UI::getItemName(uint16_t itemId) const {
    if (itemId < itemNames_.size() && !itemNames_[itemId].empty())
        return itemNames_[itemId];
    char buf[16];
    snprintf(buf, sizeof(buf), "Item %u", itemId);
    return buf;
}

std::string UI::getStarString(uint8_t stars) const {
    std::string s;
    for (int i = 0; i < stars; i++) s += '*';
    return s;
}

SDL_Color UI::getTypeColor(uint8_t type) const {
    // Standard Pokemon type colors
    constexpr SDL_Color typeColors[] = {
        {168, 168, 120, 255}, // Normal
        {192,  48,  40, 255}, // Fighting
        {168, 144, 240, 255}, // Flying
        {160,  64, 160, 255}, // Poison
        {224, 192, 104, 255}, // Ground
        {184, 160,  56, 255}, // Rock
        {168, 184,  32, 255}, // Bug
        {112,  88, 152, 255}, // Ghost
        {184, 184, 208, 255}, // Steel
        {240, 128,  48, 255}, // Fire
        {104, 144, 240, 255}, // Water
        {120, 200,  80, 255}, // Grass
        {248, 208,  48, 255}, // Electric
        {248,  88, 136, 255}, // Psychic
        {152, 216, 216, 255}, // Ice
        {112,  56, 248, 255}, // Dragon
        {112,  88,  72, 255}, // Dark
        {238, 153, 172, 255}, // Fairy
        {220, 230, 245, 255}, // Stellar
    };
    if (type < 19) return typeColors[type];
    return COLOR_TEXT;
}

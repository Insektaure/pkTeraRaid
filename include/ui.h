#pragma once
#include "raid_reader.h"
#include "account.h"
#include "text_data.h"
#include "swsh/den_crawler.h"
#include "swsh/den_locations.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>
#include <unordered_map>

enum class AppScreen { ProfileSelector, GameSelector, RaidView };

class UI {
public:
    bool init();
    void shutdown();
    void showSplash();
    void showMessageAndWait(const std::string& title, const std::string& body);
    void showWorking(const std::string& msg);
    void run(const std::string& basePath);
    void runLive(const std::string& basePath, GameVersion game);
    void runSwSh(const std::string& basePath, GameVersion game);

private:
    SDL_Window*          window_    = nullptr;
    SDL_Renderer*        renderer_  = nullptr;
    SDL_GameController*  pad_       = nullptr;
    TTF_Font*            font_      = nullptr;
    TTF_Font*            fontSmall_ = nullptr;
    TTF_Font*            fontLarge_ = nullptr;
    TTF_Font*            fontTiny_  = nullptr;

    // Screen dimensions (Switch: 1280x720)
    static constexpr int SCREEN_W = 1280;
    static constexpr int SCREEN_H = 720;

    // Colors
    static constexpr SDL_Color COLOR_BG         = {30, 30, 40, 255};
    static constexpr SDL_Color COLOR_PANEL_BG   = {45, 45, 60, 255};
    static constexpr SDL_Color COLOR_CURSOR      = {255, 220, 50, 255};
    static constexpr SDL_Color COLOR_TEXT        = {240, 240, 240, 255};
    static constexpr SDL_Color COLOR_TEXT_DIM    = {160, 160, 170, 255};
    static constexpr SDL_Color COLOR_SHINY       = {255, 215, 0, 255};
    static constexpr SDL_Color COLOR_STATUS      = {140, 200, 140, 255};
    static constexpr SDL_Color COLOR_RED         = {220, 60, 60, 255};
    static constexpr SDL_Color COLOR_ARROW       = {180, 180, 200, 255};
    static constexpr SDL_Color COLOR_MAP_DOT     = {255, 80, 80, 255};
    static constexpr SDL_Color COLOR_MAP_DOT_SEL = {255, 255, 80, 255};
    static constexpr SDL_Color COLOR_MAP_DOT_6   = {60, 60, 60, 255};
    static constexpr SDL_Color COLOR_TERA        = {100, 200, 255, 255};

    // Joystick
    static constexpr int16_t STICK_DEADZONE = 16000;
    static constexpr uint32_t STICK_INITIAL_DELAY = 400;
    static constexpr uint32_t STICK_REPEAT_DELAY  = 150;
    int stickDirX_ = 0, stickDirY_ = 0;
    uint32_t stickMoveTime_ = 0;
    bool stickMoved_ = false;
    void updateStick(int16_t axisX, int16_t axisY);

    // App state
    AppScreen screen_ = AppScreen::GameSelector;
    std::string basePath_;
    bool liveMode_ = false;

    // Account
    AccountManager account_;
    int profileSelCursor_ = 0;
    int selectedProfile_ = -1;

    // Game selector (Scarlet / Violet)
    int gameSelCursor_ = 0;
    std::vector<GameVersion> availableGames_;
    std::unordered_map<GameVersion, SDL_Texture*> gameIconCache_;
    void loadGameIcons();
    void freeGameIcons();

    // Raid data
    RaidReader reader_;
    GameVersion selectedVersion_ = GameVersion::Scarlet;

    // Text data
    std::vector<std::string> speciesNames_;
    std::vector<std::string> moveNames_;
    std::vector<std::string> natureNames_;
    std::vector<std::string> abilityNames_;
    std::vector<std::string> typeNames_;

    // Sprite cache: species ID -> texture
    std::unordered_map<uint16_t, SDL_Texture*> spriteCache_;
    SDL_Texture* getSprite(uint16_t species);
    void freeSprites();

    // Map textures
    SDL_Texture* mapPaldea_    = nullptr;
    SDL_Texture* mapKitakami_  = nullptr;
    SDL_Texture* mapBlueberry_ = nullptr;

    // Raid view state
    TeraRaidMapParent currentMap_ = TeraRaidMapParent::Paldea;
    int raidCursor_ = 0;
    int raidScroll_ = 0;
    bool showDetail_ = false;
    bool showAbout_  = false;

    // Layout constants for raid view
    // Map is rendered as a square using full available height
    static constexpr int MAP_PANEL_X = 5;
    static constexpr int MAP_PANEL_Y = 45;
    static constexpr int MAP_PANEL_H = 635;
    static constexpr int MAP_PANEL_W = 605;
    static constexpr int LIST_PANEL_X = 615;
    static constexpr int LIST_PANEL_W = 660;
    static constexpr int LIST_PANEL_Y = 45;
    static constexpr int LIST_PANEL_H = 635;
    static constexpr int LIST_SPRITE_SIZE = 52;
    static constexpr int TAB_H = 35;

    // Filtered raids for current map
    std::vector<int> filteredIndices_;
    void rebuildFilteredList();

    // Profile selector
    void drawProfileSelectorFrame();
    void handleProfileSelectorInput(bool& running);
    void selectProfile(int index);

    // Game selector
    void drawGameSelectorFrame();
    void handleGameSelectorInput(bool& running);
    void selectGame(GameVersion game);

    // Raid view
    void drawRaidViewFrame();
    void handleRaidViewInput(bool& running);
    void drawMapPanel();
    void drawListPanel();
    void drawDetailPopup(const RaidInfo& raid);
    void drawRaidRow(int x, int y, int w, const RaidInfo& raid, bool selected, int rowH);

    // About popup
    void drawAboutPopup();

    // Map tabs
    void drawMapTabs(int x, int y, int w);

    // Rendering helpers
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f);
    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f);
    void drawTextRight(const std::string& text, int rx, int y, SDL_Color color, TTF_Font* f);
    void drawRect(int x, int y, int w, int h, SDL_Color color);
    void drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness);
    void drawStatusBar(const std::string& msg);
    void fillCircle(int cx, int cy, int r, SDL_Color color);

    // Name lookups
    std::string getSpeciesName(uint16_t species) const;
    std::string getMoveName(uint16_t move) const;
    std::string getNatureName(uint8_t nature) const;
    std::string getAbilityName(int ability) const;
    std::string getTypeName(uint8_t type) const;
    std::string getStarString(uint8_t stars) const;
    SDL_Color getTypeColor(uint8_t type) const;

    // --- SwSh Den Crawler ---
    DenCrawler denCrawler_;
    int swshTab_ = 0;       // 0=Wild Area, 1=IoA, 2=Crown Tundra
    int swshCursor_ = 0;
    int swshScroll_ = 0;
    bool swshShowDetail_ = false;
    std::vector<int> swshFiltered_;  // indices into denCrawler_.dens()

    // SwSh map textures
    SDL_Texture* mapWildArea_ = nullptr;
    SDL_Texture* mapIoA_      = nullptr;
    SDL_Texture* mapCT_       = nullptr;

    void rebuildSwShFilteredList();
    void drawSwShViewFrame();
    void drawSwShMapPanel();
    void drawSwShMapTabs(int x, int y, int w);
    void drawSwShListPanel();
    void drawSwShRow(int x, int y, int w, const SwShDenInfo& den, bool selected, int rowH);
    void drawSwShDetailPopup(const SwShDenInfo& den);
    void handleSwShViewInput(bool& running);
};

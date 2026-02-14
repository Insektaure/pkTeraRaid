#include "ui.h"
#include "game_type.h"
#include "swsh/den_crawler.h"

#ifdef __SWITCH__
#include <switch.h>
#include "dmnt_mem.h"
#endif

#include <cstdio>

#include <string>

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    romfsInit();
#endif

    // Determine base path
    std::string basePath;
#ifdef __SWITCH__
    if (argc > 0 && argv[0]) {
        basePath = argv[0];
        auto lastSlash = basePath.rfind('/');
        if (lastSlash != std::string::npos)
            basePath = basePath.substr(0, lastSlash + 1);
        else
            basePath = "sdmc:/switch/pkTeraRaid/";
    } else {
        basePath = "sdmc:/switch/pkTeraRaid/";
    }
#else
    basePath = "./";
#endif

    UI ui;
    if (!ui.init()) {
#ifdef __SWITCH__
        romfsExit();
#endif
        return 1;
    }

    ui.showSplash();

#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    bool liveMode = (at != AppletType_Application && at != AppletType_SystemApplication);

    if (liveMode) {
        // Applet mode: read from live game memory via dmntcht
        if (!DmntMem::init()) {
            ui.showMessageAndWait("dmntcht Error",
                                  "Could not connect to dmnt:cht.\n"
                                  "Make sure Atmosphere is running.");
            ui.shutdown();
            romfsExit();
            return 1;
        }

        uint64_t tid = DmntMem::titleId();
        GameVersion game;
        if (tid == SCARLET_TITLE_ID) {
            game = GameVersion::Scarlet;
        } else if (tid == VIOLET_TITLE_ID) {
            game = GameVersion::Violet;
        } else if (tid == SWORD_TITLE_ID) {
            game = GameVersion::Sword;
        } else if (tid == SHIELD_TITLE_ID) {
            game = GameVersion::Shield;
        } else {
            ui.showMessageAndWait("Wrong Game",
                                  "No supported game is running.\n"
                                  "Please launch Pokemon Sword, Shield,\n"
                                  "Scarlet, or Violet.");
            DmntMem::exit();
            ui.shutdown();
            romfsExit();
            return 1;
        }

        if (isSwSh(game)) {
            DenCrawler crawler;
            if (crawler.readLive(game)) {
                int active = 0;
                for (auto& d : crawler.dens())
                    if (d.isActive) active++;

                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Read %d active dens (of %d total) from %s.",
                         active, (int)crawler.dens().size(),
                         gameDisplayNameOf(game));
                ui.showMessageAndWait("SwSh Den Crawler", msg);
            } else {
                ui.showMessageAndWait("Error",
                                      "Failed to read den data from memory.");
            }
        } else {
            ui.runLive(basePath, game);
        }
        DmntMem::exit();
    } else {
        // Title override mode: read from save file
        ui.run(basePath);
    }
#else
    ui.run(basePath);
#endif

    ui.shutdown();

#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}

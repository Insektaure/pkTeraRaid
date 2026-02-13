#pragma once
#include "game_type.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdint>

#ifdef __SWITCH__
#include <switch.h>
#endif

struct UserProfile {
    std::string nickname;
    std::string pathSafeName;
#ifdef __SWITCH__
    AccountUid uid{};
#endif
    SDL_Texture* iconTexture = nullptr;
};

class AccountManager {
public:
    bool init();
    void shutdown();

    bool loadProfiles(SDL_Renderer* renderer);
    bool hasSaveData(int profileIndex, GameVersion game) const;
    std::string mountSave(int profileIndex, GameVersion game);
    void unmountSave();

    const std::vector<UserProfile>& profiles() const { return users_; }
    int profileCount() const { return (int)users_.size(); }
    void freeTextures();

private:
    std::vector<UserProfile> users_;
    bool mounted_ = false;

#ifdef __SWITCH__
    FsFileSystem saveFs_{};
    bool loadProfile(AccountUid uid, SDL_Renderer* renderer, UserProfile& out);
    static std::string sanitizeForPath(const char* nickname);
#endif
};

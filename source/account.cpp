#include "account.h"
#include <cstring>

#ifdef __SWITCH__
#include <SDL2/SDL_image.h>
#endif

bool AccountManager::init() {
#ifdef __SWITCH__
    return R_SUCCEEDED(accountInitialize(AccountServiceType_Administrator));
#else
    return false;
#endif
}

void AccountManager::shutdown() {
#ifdef __SWITCH__
    unmountSave();
    freeTextures();
    accountExit();
#endif
}

bool AccountManager::loadProfiles(SDL_Renderer* renderer) {
#ifdef __SWITCH__
    AccountUid accounts[8];
    int32_t total = 0;
    Result rc = accountListAllUsers(accounts, 8, &total);
    if (R_FAILED(rc) || total <= 0)
        return false;

    users_.clear();
    for (int i = 0; i < total; i++) {
        UserProfile profile;
        if (loadProfile(accounts[i], renderer, profile))
            users_.push_back(std::move(profile));
    }
    return !users_.empty();
#else
    (void)renderer;
    return false;
#endif
}

#ifdef __SWITCH__
bool AccountManager::loadProfile(AccountUid uid, SDL_Renderer* renderer, UserProfile& out) {
    out.uid = uid;

    AccountProfile profile;
    if (R_FAILED(accountGetProfile(&profile, uid)))
        return false;

    AccountProfileBase base;
    if (R_FAILED(accountProfileGet(&profile, nullptr, &base))) {
        accountProfileClose(&profile);
        return false;
    }
    out.nickname = base.nickname;
    out.pathSafeName = sanitizeForPath(base.nickname);

    u32 imgSize = 0;
    if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imgSize)) && imgSize > 0) {
        std::vector<uint8_t> jpegBuf(imgSize);
        u32 actualSize = 0;
        if (R_SUCCEEDED(accountProfileLoadImage(&profile, jpegBuf.data(), imgSize, &actualSize))) {
            SDL_RWops* rw = SDL_RWFromMem(jpegBuf.data(), (int)actualSize);
            if (rw) {
                SDL_Surface* surf = IMG_Load_RW(rw, 1);
                if (surf) {
                    out.iconTexture = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_FreeSurface(surf);
                }
            }
        }
    }

    accountProfileClose(&profile);
    return true;
}

std::string AccountManager::sanitizeForPath(const char* nickname) {
    std::string result;
    for (const char* p = nickname; *p; p++) {
        char c = *p;
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            continue;
        if (c >= 0x20)
            result += c;
    }
    size_t start = result.find_first_not_of(' ');
    if (start == std::string::npos) return "User";
    size_t end = result.find_last_not_of(' ');
    result = result.substr(start, end - start + 1);
    if (result.empty()) return "User";
    if (result.size() > 32) result = result.substr(0, 32);
    return result;
}
#endif

bool AccountManager::hasSaveData(int profileIndex, GameVersion game) const {
#ifdef __SWITCH__
    if (profileIndex < 0 || profileIndex >= (int)users_.size())
        return false;

    FsSaveDataAttribute attr{};
    attr.application_id = titleIdOf(game);
    attr.uid = users_[profileIndex].uid;
    attr.save_data_type = FsSaveDataType_Account;

    FsFileSystem tempFs;
    Result rc = fsOpenSaveDataFileSystem(&tempFs, FsSaveDataSpaceId_User, &attr);
    if (R_SUCCEEDED(rc)) {
        fsFsClose(&tempFs);
        return true;
    }
    return false;
#else
    (void)profileIndex;
    (void)game;
    return true;
#endif
}

std::string AccountManager::mountSave(int profileIndex, GameVersion game) {
#ifdef __SWITCH__
    if (profileIndex < 0 || profileIndex >= (int)users_.size())
        return "";

    if (mounted_)
        unmountSave();

    FsSaveDataAttribute attr{};
    attr.application_id = titleIdOf(game);
    attr.uid = users_[profileIndex].uid;
    attr.save_data_type = FsSaveDataType_Account;

    Result rc = fsOpenSaveDataFileSystem(&saveFs_, FsSaveDataSpaceId_User, &attr);
    if (R_FAILED(rc))
        return "";

    int devRc = fsdevMountDevice("save", saveFs_);
    if (devRc < 0) {
        fsFsClose(&saveFs_);
        return "";
    }

    mounted_ = true;
    return "save:/";
#else
    (void)profileIndex;
    (void)game;
    return "";
#endif
}

void AccountManager::unmountSave() {
#ifdef __SWITCH__
    if (mounted_) {
        fsdevUnmountDevice("save");
        mounted_ = false;
    }
#endif
}

void AccountManager::freeTextures() {
    for (auto& user : users_) {
        if (user.iconTexture) {
            SDL_DestroyTexture(user.iconTexture);
            user.iconTexture = nullptr;
        }
    }
}

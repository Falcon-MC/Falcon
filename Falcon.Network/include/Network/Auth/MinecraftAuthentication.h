#pragma once

#include "Network/Auth/LiveAuthentication.h"
#include "Network/Auth/XboxLiveAuthentication.h"
#include "Network/Auth/XboxLiveConfig.h"

#include <cstdint>
#include <mutex>
#include <string>

class KeyPair;

struct MinecraftAuthenticationResult {
    std::string mChainJson;
    std::string mMultiplayerToken;
    std::string mDisplayName;
    std::string mIdentity;
    std::string mXuid;
    std::string mTitleId;
    std::string mPlayFabId;
};

class MinecraftAuthentication {
public:
    MinecraftAuthentication(const XboxLiveConfig &config, const std::string &cacheFilePath,
                            const std::string &gameVersion);

    LiveAuthentication &getLiveAuthentication() {
        return mLive;
    }

    XboxLiveAuthentication &getXboxLiveAuthentication() {
        return mXbox;
    }

    const std::string &getGameVersion() const {
        return mGameVersion;
    }

    bool authenticate(const KeyPair &key, bool legacy, MinecraftAuthenticationResult &outResult,
                      std::string &outError);

    bool requestChain(const KeyPair &key, std::string &outChainJson, std::string &outError);

    bool requestMultiplayerToken(const KeyPair &key, std::string &outToken, std::string &outError);

    bool requestServiceToken(std::string &outAuthorization, std::string &outError);

    bool requestServiceUri(const std::string &serviceName, std::string &outServiceUri, std::string &outError);

    static bool readChainIdentity(const std::string &chainJson, MinecraftAuthenticationResult &outResult,
                                  std::string &outError);

private:
    struct Environment {
        std::string mServiceUri;
        std::string mIssuer;
        std::string mPlayFabTitleId;
        std::string mDiscoveryBody;
        bool mLoaded = false;
    };

    bool _discover(std::string &outError);

    bool _loginPlayFab(std::string &outSessionTicket, std::string &outError);

    bool _serviceToken(std::string &outAuthorization, std::string &outError);

    std::string _userConfigJson(const std::string &sessionTicket) const;

    bool _readServiceToken(const std::string &body, std::string &outError);

    LiveAuthentication mLive;
    XboxLiveAuthentication mXbox;
    std::string mGameVersion;
    Environment mEnvironment;
    std::string mServiceAuthorization;
    int64_t mServiceValidUntil;
    std::mutex mMutex;
};

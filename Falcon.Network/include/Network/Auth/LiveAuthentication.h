#pragma once

#include "Network/Auth/XboxLiveConfig.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

struct LiveToken {
    std::string mAccessToken;
    std::string mRefreshToken;
    std::string mTokenType;
    int64_t mExpiry = 0;

    bool isValid() const;
};

class LiveAuthentication {
public:
    typedef std::function<void(const std::string &verificationUri, const std::string &userCode)> DeviceCodeCallback;

    LiveAuthentication(const XboxLiveConfig &config, const std::string &cacheFilePath);

    const XboxLiveConfig &getConfig() const {
        return mConfig;
    }

    void setDeviceCodeCallback(const DeviceCodeCallback &callback);

    void setCancelFlag(const std::atomic<bool> *cancel);

    bool getToken(LiveToken &outToken, std::string &outError);

    bool requestDeviceCodeToken(LiveToken &outToken, std::string &outError);

    bool refreshToken(const LiveToken &token, LiveToken &outToken, std::string &outError);

    bool loadCache();

    bool saveCache() const;

    void clearCache();

private:
    bool _isCancelled() const;

    static bool _parseTokenResponse(const std::string &body, LiveToken &outToken, std::string &outErrorCode,
                                    std::string &outErrorDescription);

    XboxLiveConfig mConfig;
    std::string mCacheFilePath;
    DeviceCodeCallback mDeviceCodeCallback;
    const std::atomic<bool> *mCancel;
    LiveToken mToken;
    mutable std::mutex mMutex;
};

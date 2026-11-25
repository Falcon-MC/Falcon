#pragma once

#include "Network/Auth/LiveAuthentication.h"
#include "Network/Auth/XboxLiveConfig.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

struct HttpResponse;

struct XboxLiveToken {
    std::string mGamerTag;
    std::string mXuid;
    std::string mUserHash;
    std::string mToken;
    int64_t mIssueInstant = 0;
    int64_t mNotAfter = 0;

    bool isValid() const;

    std::string getAuthorizationHeader() const;
};

class XboxLiveAuthentication {
public:
    static const char *MULTIPLAYER_RELYING_PARTY;
    static const char *PLAYFAB_RELYING_PARTY;

    explicit XboxLiveAuthentication(LiveAuthentication &live);

    ~XboxLiveAuthentication();

    XboxLiveAuthentication(const XboxLiveAuthentication &) = delete;

    XboxLiveAuthentication &operator=(const XboxLiveAuthentication &) = delete;

    bool requestToken(const std::string &relyingParty, XboxLiveToken &outToken, std::string &outError);

    static void updateServerTime(const HttpResponse &response);

    static std::string describeErrorCode(const std::string &code);

private:
    class ProofKey;

    struct DeviceToken {
        std::string mToken;
        int64_t mNotAfter = 0;

        bool isValid() const;
    };

    bool _requestDeviceToken(std::string &outError);

    bool _authorize(const LiveToken &liveToken, const std::string &relyingParty, XboxLiveToken &outToken,
                    std::string &outError);

    LiveAuthentication &mLive;
    std::unique_ptr<ProofKey> mProofKey;
    DeviceToken mDeviceToken;
    std::map<std::string, XboxLiveToken> mTokens;
    std::mutex mMutex;
};

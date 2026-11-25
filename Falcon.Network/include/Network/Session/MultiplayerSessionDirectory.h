#pragma once

#include "Network/NetherNet/XboxRtaClient.h"
#include "Network/Session/SessionConnectionTarget.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class MinecraftAuthentication;

struct XboxLiveToken;

struct MultiplayerConnection {
    static const int TYPE_SIGNALING_OVER_WEBSOCKET = 3;
    static const int TYPE_SIGNALING_OVER_LAN = 4;
    static const int TYPE_SIGNALING_OVER_JSON_RPC = 7;

    int mType = 0;
    std::string mHostIpAddress;
    unsigned short mHostPort = 0;
    std::string mNetherNetId;
    std::string mPlayerMessagingId;

    bool validate(std::string &outError) const;
};

struct MultiplayerWorld {
    static const int TRANSPORT_LAYER_RAKNET = 0;
    static const int TRANSPORT_LAYER_NETHERNET = 2;

    std::string mHandleId;
    std::string mOwnerXuid;
    std::string mOwnerId;
    std::string mHostName;
    std::string mWorldName;
    std::string mVersion;
    int mProtocol = 0;
    int mMemberCount = 0;
    int mMaxMemberCount = 0;
    int mTransportLayer = 0;
    long long mRealmId = 0;
    std::vector<MultiplayerConnection> mSupportedConnections;
    std::map<std::string, std::string> mNonces;

    static bool parse(const std::string &customProperties, MultiplayerWorld &outWorld, std::string &outError);

    bool selectConnection(SessionConnectionTarget &outTarget, std::string &outError) const;
};

class MultiplayerSessionDirectory {
public:
    explicit MultiplayerSessionDirectory(MinecraftAuthentication &authentication);

    ~MultiplayerSessionDirectory();

    MultiplayerSessionDirectory(const MultiplayerSessionDirectory &) = delete;

    MultiplayerSessionDirectory &operator=(const MultiplayerSessionDirectory &) = delete;

    bool queryWorlds(std::vector<MultiplayerWorld> &outWorlds, std::string &outError);

    bool join(const std::string &handleId, unsigned int timeoutMs, const std::atomic<bool> *cancel,
              SessionConnectionTarget &outTarget, std::string &outError);

    bool leave(std::string &outError);

    bool isJoined() const;

    const MultiplayerWorld &getWorld() const;

private:
    bool _authorize(XboxLiveToken &outToken, std::string &outError);

    bool _subscribe(const XboxLiveToken &token, std::string &outConnectionId, std::string &outError);

    bool _sync(std::string &outError);

    bool _applySession(const std::string &body, bool &outReady, std::string &outError);

    void _onEvent(unsigned int subscriptionId, const std::string &custom);

    void _release();

    MinecraftAuthentication &mAuthentication;
    nethernet::XboxRtaClient mRta;
    std::string mXuid;
    std::string mAuthorization;

    std::string mSessionUrl;
    std::string mEtag;
    MultiplayerWorld mWorld;
    SessionConnectionTarget mTarget;
    bool mHasConnection;
    bool mJoined;

    std::mutex mMutex;
    std::condition_variable mSignal;
    std::string mSessionReference;
    unsigned int mSubscriptionId;
    bool mSubscribed;
    bool mChanged;
};

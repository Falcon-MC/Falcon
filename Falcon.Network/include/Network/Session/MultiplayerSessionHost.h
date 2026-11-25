#pragma once

#include "Network/Client/ClientNetworkSystem.h"
#include "Network/NetherNet/XboxRtaClient.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class MinecraftAuthentication;

struct XboxLiveToken;

struct HostedWorld {
    std::string mWorldName;
    std::string mHostName;
    std::string mVersion;
    int mProtocol = 0;
    int mMemberCount = 0;
    int mMaxMemberCount = 0;
    NetherNetSignalingType mSignalingType = NetherNetSignalingType::JsonRpc;
    std::string mNetherNetId;
    std::string mPlayerMessagingId;
};

class MultiplayerSessionHost {
public:
    explicit MultiplayerSessionHost(MinecraftAuthentication &authentication);

    ~MultiplayerSessionHost();

    MultiplayerSessionHost(const MultiplayerSessionHost &) = delete;

    MultiplayerSessionHost &operator=(const MultiplayerSessionHost &) = delete;

    bool publish(const HostedWorld &world, std::string &outError);

    void update(const HostedWorld &world);

    bool validateNonce(const std::string &xuid, const std::string &nonce) const;

    void close();

    bool isPublished() const { return mPublished.load(); }

    /**
     * How many attempts in a row to restore the session after it was lost have failed. Reset once one succeeds.
     */
    int getFailedRecoveries() const { return mFailedRecoveries.load(); }

private:
    bool _authorize(std::string &outError);

    bool _subscribe(std::string &outConnectionId, std::string &outError);

    bool _createSession(const std::string &connectionId, std::string &outError);

    bool _writeActivity(std::string &outError);

    bool _put(const std::string &body, bool create, std::string &outBody, bool &outDeleted, std::string &outError);

    bool _sync(std::string &outError);

    bool _publishCustom(std::string &outError);

    bool _updateConnection(const std::string &connectionId, std::string &outError);

    void _assignNonces(const std::string &sessionBody);

    std::string _customProperties() const;

    void _onEvent(unsigned int subscriptionId, const std::string &custom);

    void _maintain();

    bool _recover(std::string &outError);

    bool _restore(std::string &outError);

    MinecraftAuthentication &mAuthentication;
    nethernet::XboxRtaClient mRta;
    std::string mXuid;
    std::string mAuthorization;
    std::string mSessionName;
    std::string mSessionUrl;
    std::string mSessionReference;
    std::string mLevelId;
    std::string mConnectionId;
    HostedWorld mWorld;
    std::map<std::string, std::string> mNonces;
    std::string mPublishedCustom;
    unsigned int mSubscriptionId;
    bool mChanged;
    bool mWorldChanged;
    std::atomic<bool> mPublished;
    std::atomic<bool> mStopping;
    std::atomic<int> mFailedRecoveries{0};
    std::thread mThread;
    mutable std::mutex mMutex;
    std::mutex mRequestMutex;
    std::condition_variable mSignal;
};

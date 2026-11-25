#pragma once

#include "Network/BedrockConnection.h"
#include "Network/Client/ClientConnectionRequest.h"
#include "Network/Client/ClientNetworkSystem.h"
#include "Network/Connector.h"
#include "Network/RakPeerHelper.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class PacketCodecContext;

class ServerTransport;

class MinecraftAuthentication;

class MultiplayerSessionHost;

class NetherNetInstance;

namespace nethernet {
    class Signaling;
}

struct ListenerSettings {
    unsigned short mPort = 19132;
    unsigned short mPortV6 = 19133;
    int mMaxPlayers = 10;
    std::string mServerName = "Bedrock Server";
    std::string mSubName = "Bedrock Server";
    int mProtocolVersion = 0;
    std::string mGameVersion;
    unsigned short mCompressionThreshold = 1;
    bool mEncryption = true;
    bool mRakNet = true;
    bool mNetherNet = false;
    std::string mLevelName = "Bedrock level";
    unsigned int mLoginTimeoutMs = 30000;
    const PacketCodecContext *mCodecContext = nullptr;
    MinecraftAuthentication *mAuthentication = nullptr;
    NetherNetSignalingType mOnlineSignaling = NetherNetSignalingType::JsonRpc;
    bool mPublishSession = true;
    bool mDeferResourcePacks = false;
};

struct IncomingConnection {
    std::unique_ptr<BedrockConnection> mConnection;
    ClientIdentityData mIdentity;
    std::string mLanguageCode;
    int mDeviceOS = -1;
    std::string mGameVersion;
    std::string mAuthJwt;
    std::string mClientJwt;
    int mProtocolVersion = 0;
};

class BedrockListener : public Connector::ConnectionCallbacks, public RakPeerHelper::IPSupportInterface {
public:
    BedrockListener();

    ~BedrockListener() override;

    BedrockListener(const BedrockListener &) = delete;

    BedrockListener &operator=(const BedrockListener &) = delete;

    bool listen(const ListenerSettings &settings, std::string &outError);

    bool accept(IncomingConnection &outConnection, int timeoutMs = -1);

    void close();

    /**
     * Replaces what the published multiplayer session shows to friends. A negative player count keeps showing
     * the players connected to this listener.
     */
    void advertise(const std::string &serverName, const std::string &subName, int playerCount, int maxPlayers);

    bool isListening() const {
        return mRunning.load();
    }

    /**
     * Whether friends can still reach the listener: the NetherNet signaling is connected and the multiplayer
     * session is published. outReason says what is missing otherwise.
     */
    bool isOnline(std::string &outReason) const;

    bool onValidateIncomingConnection(const NetworkIdentifier &id) override;

    void onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) override;

    void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                            const std::string &message) override;

    void onReceiveIPSupport(RakPeerHelper::IPSupport support) override;

private:
    enum class LoginState : int {
        WaitingNetworkSettings,
        WaitingLogin,
        WaitingHandshake,
        WaitingResourcePacks
    };

    struct PendingLogin {
        IncomingConnection mIncoming;
        std::shared_ptr<ServerTransport> mTransport;
        LoginState mState = LoginState::WaitingNetworkSettings;
        std::chrono::steady_clock::time_point mStarted;
        bool mRequiresNonce = false;
    };

    void _run();

    void _tickLogins();

    bool _handleLoginPacket(PendingLogin &login, std::string payload);

    bool _handleNetworkSettings(PendingLogin &login, std::string payload);

    bool _handleLogin(PendingLogin &login, std::string payload);

    void _completeLogin(PendingLogin &login);

    void _finishLogin(PendingLogin &login);

    bool _addConnector(TransportLayer layer, std::string &outError);

    std::shared_ptr<ServerTransport> _createTransport(const NetworkIdentifier &id,
                                                      const std::shared_ptr<NetworkPeer> &peer) const;

    void _updatePlayerCount();

    void _goOnline(NetherNetInstance &instance);

    ListenerSettings mSettings;
    std::shared_ptr<nethernet::Signaling> mSignaling;
    std::unique_ptr<MultiplayerSessionHost> mSessionHost;
    std::string mNetherNetId;
    std::string mPlayerMessagingId;
    int mPublishedPlayerCount = -1;
    std::mutex mAdvertisementMutex;
    std::string mAdvertisedServerName;
    std::string mAdvertisedSubName;
    int mAdvertisedPlayerCount = -1;
    int mAdvertisedMaxPlayers = 0;
    std::atomic<bool> mAdvertisementChanged{false};
    std::vector<std::unique_ptr<Connector>> mConnectors;
    std::thread mThread;
    std::atomic<bool> mRunning;
    std::atomic<int> mPlayerCount;

    std::unordered_map<NetworkIdentifier, std::unique_ptr<PendingLogin>, NetworkIdentifier::Hasher> mPending;
    std::unordered_map<NetworkIdentifier, std::weak_ptr<ServerTransport>, NetworkIdentifier::Hasher> mActive;

    std::mutex mAcceptMutex;
    std::condition_variable mAcceptCondition;
    std::deque<IncomingConnection> mAccepted;
};

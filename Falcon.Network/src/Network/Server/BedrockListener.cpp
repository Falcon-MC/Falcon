#include "Network/Server/BedrockListener.h"

#include "Network/ConnectionRequest.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/PingedCompatibleServer.h"
#include "Network/NetherNet/NetherNetConnection.h"
#include "Network/NetherNet/NetherNetInstance.h"
#include "Network/NetherNet/NetherNetJsonRpcSignaling.h"
#include "Network/NetherNet/NetherNetWebSocketSignaling.h"
#include "Network/Session/MultiplayerSessionHost.h"
#include "Core/Debug/BedrockLog.h"
#include "Network/RakNetInstance.h"
#include "Network/Server/NetherNetServerTransport.h"
#include "Network/Server/RakNetServerTransport.h"
#include "Network/TransportFactory.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"

#include <random>
#include <utility>
#include <vector>

namespace {

    const int LISTENER_IDLE_WAIT_MS = 1;
    const char *GAME_MODE_NAME = "Survival";
    const int SERVER_AVAILABILITY = 1;
    const char *GAME_VERSION_KEY = "GameVersion";
    const char *NONCE_KEY = "Nonce";
    const int MAX_FAILED_SESSION_RECOVERIES = 3;

}

BedrockListener::BedrockListener() : mRunning(false) {
}

BedrockListener::~BedrockListener() {
    close();
}

bool BedrockListener::listen(const ListenerSettings &settings, std::string &outError) {
    if (mRunning.load()) {
        outError = "the listener is already running";
        return false;
    }

    mSettings = settings;
    mPlayerCount.store(0);

    if (!settings.mRakNet && !settings.mNetherNet) {
        outError = "no transport enabled";
        return false;
    }

    if ((settings.mRakNet && !_addConnector(TransportLayer::RakNet, outError))
        || (settings.mNetherNet && !_addConnector(TransportLayer::NetherNet, outError))) {
        for (std::unique_ptr<Connector> &connector: mConnectors)
            connector->disconnect();
        mConnectors.clear();
        return false;
    }

    mRunning.store(true);
    mThread = std::thread(&BedrockListener::_run, this);
    return true;
}

bool BedrockListener::_addConnector(TransportLayer layer, std::string &outError) {
    std::unique_ptr<Connector> connector = TransportFactory::createConnector(layer, *this, true);
    if (connector == nullptr) {
        outError = std::string("could not create the ") + toString(layer) + " transport";
        return false;
    }

    connector->setCallbacks(this);

    NetherNetInstance *netherNet = dynamic_cast<NetherNetInstance *>(connector.get());
    if (netherNet != nullptr) {
        netherNet->setServerDataProvider([this]() {
            nethernet::ServerData data;
            data.mServerName = mSettings.mServerName;
            data.mProtocol = mSettings.mProtocolVersion;
            data.mGameVersion = mSettings.mGameVersion;
            data.mLevelName = mSettings.mLevelName;
            data.mPlayerCount = mPlayerCount.load();
            data.mMaxPlayerCount = mSettings.mMaxPlayers;
            return data;
        });
    }

    if (!connector->host(ConnectionDefinition::createFromPorts(mSettings.mPort, mSettings.mPortV6,
                                                               mSettings.mMaxPlayers))) {
        outError = std::string("could not listen with ") + toString(layer) + " on port "
                   + std::to_string(mSettings.mPort);
        return false;
    }

    if (netherNet != nullptr && mSettings.mAuthentication != nullptr)
        _goOnline(*netherNet);

    RakNetInstance *rakNet = dynamic_cast<RakNetInstance *>(connector.get());
    if (rakNet != nullptr) {
        const ListenerSettings &settings = mSettings;
        PingedCompatibleServer announcement;
        announcement.mServerName = settings.mServerName;
        announcement.mSubName = settings.mSubName;
        announcement.mGameVersion = settings.mGameVersion;
        announcement.mGameMode = GAME_MODE_NAME;
        announcement.mServerAvailability = SERVER_AVAILABILITY;
        announcement.mProtocolVersion = settings.mProtocolVersion;
        announcement.mMaxPlayers = settings.mMaxPlayers;
        announcement.mPort = settings.mPort;
        announcement.mServerId = ((unsigned long long) std::random_device{}() << 32) | std::random_device{}();
        rakNet->announceServer(announcement);
    }

    mConnectors.push_back(std::move(connector));
    return true;
}

void BedrockListener::_goOnline(NetherNetInstance &instance) {
    std::string error;
    mNetherNetId = std::to_string(instance.getNetworkId());

    if (mSettings.mOnlineSignaling == NetherNetSignalingType::WebSocket) {
        std::shared_ptr<nethernet::WebSocketSignaling> signaling = std::make_shared<nethernet::WebSocketSignaling>();
        if (!signaling->connect(*mSettings.mAuthentication, mNetherNetId, error)) {
            LOG_WARN(LogAreaID::Network, "Could not connect the NetherNet signaling: %s", error.c_str());
            return;
        }
        mSignaling = signaling;
    } else {
        std::shared_ptr<nethernet::JsonRpcSignaling> signaling = std::make_shared<nethernet::JsonRpcSignaling>();
        if (!signaling->connect(*mSettings.mAuthentication, mNetherNetId, error)) {
            LOG_WARN(LogAreaID::Network, "Could not connect the NetherNet signaling: %s", error.c_str());
            return;
        }
        mPlayerMessagingId = signaling->getPlayerMessagingID();
        mSignaling = signaling;
    }

    instance.attachSignaling(mSignaling);
    LOG_INFO(LogAreaID::Network, "NetherNet signaling is online as %s",
             mPlayerMessagingId.empty() ? mNetherNetId.c_str() : mPlayerMessagingId.c_str());

    if (!mSettings.mPublishSession)
        return;

    HostedWorld world;
    world.mWorldName = mSettings.mServerName;
    world.mHostName = mSettings.mSubName;
    world.mVersion = mSettings.mGameVersion;
    world.mProtocol = mSettings.mProtocolVersion;
    world.mMaxMemberCount = mSettings.mMaxPlayers;
    world.mSignalingType = mSettings.mOnlineSignaling;
    world.mNetherNetId = mNetherNetId;
    world.mPlayerMessagingId = mPlayerMessagingId;

    std::unique_ptr<MultiplayerSessionHost> host(new MultiplayerSessionHost(*mSettings.mAuthentication));
    if (!host->publish(world, error)) {
        LOG_WARN(LogAreaID::Network, "Could not publish the multiplayer session: %s", error.c_str());
        return;
    }

    mSessionHost = std::move(host);
    mPublishedPlayerCount = 0;
}

bool BedrockListener::accept(IncomingConnection &outConnection, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mAcceptMutex);
    const auto ready = [this]() {
        return !mAccepted.empty() || !mRunning.load();
    };

    if (timeoutMs < 0)
        mAcceptCondition.wait(lock, ready);
    else
        mAcceptCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), ready);

    if (mAccepted.empty())
        return false;

    outConnection = std::move(mAccepted.front());
    mAccepted.pop_front();
    return true;
}

void BedrockListener::close() {
    if (!mRunning.exchange(false))
        return;

    mAcceptCondition.notify_all();
    if (mThread.joinable())
        mThread.join();

    mPending.clear();
    mActive.clear();

    if (mSessionHost != nullptr) {
        mSessionHost->close();
        mSessionHost.reset();
    }

    for (std::unique_ptr<Connector> &connector: mConnectors)
        connector->disconnect();
    mConnectors.clear();
    mSignaling.reset();
}

bool BedrockListener::onValidateIncomingConnection(const NetworkIdentifier &id) {
    (void) id;
    return (int) (mPending.size() + mActive.size()) < mSettings.mMaxPlayers;
}

std::shared_ptr<ServerTransport> BedrockListener::_createTransport(const NetworkIdentifier &id,
                                                                  const std::shared_ptr<NetworkPeer> &peer) const {
    std::shared_ptr<nethernet::Connection> netherNet = std::dynamic_pointer_cast<nethernet::Connection>(peer);
    if (netherNet != nullptr)
        return std::make_shared<NetherNetServerTransport>(netherNet);

    for (const std::unique_ptr<Connector> &connector: mConnectors) {
        RakNetInstance *rakNet = dynamic_cast<RakNetInstance *>(connector.get());
        if (rakNet != nullptr)
            return std::make_shared<RakNetServerTransport>(rakNet->getPeer(), id.getGuid());
    }

    return nullptr;
}

void BedrockListener::onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) {
    std::shared_ptr<ServerTransport> transport = _createTransport(id, peer);
    if (transport == nullptr)
        return;

    std::unique_ptr<PendingLogin> login(new PendingLogin());
    login->mTransport = std::move(transport);
    login->mIncoming.mConnection.reset(new BedrockConnection(BedrockConnection::Side::Server, std::move(peer),
                                                             std::shared_ptr<ClientTransport>(login->mTransport)));
    login->mIncoming.mConnection->setCodecContext(mSettings.mCodecContext);
    login->mStarted = std::chrono::steady_clock::now();
    login->mRequiresNonce = mSessionHost != nullptr && id.getType() == NetworkIdentifier::Type::NetherNet
                            && nethernet::JsonRpcSignaling::isMessagingID(id.getNetworkID());
    mPending[id] = std::move(login);
}

void BedrockListener::onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                         const std::string &message) {
    (void) message;

    auto pending = mPending.find(id);
    if (pending != mPending.end()) {
        pending->second->mTransport->markClosed(reason);
        mPending.erase(pending);
        return;
    }

    auto active = mActive.find(id);
    if (active == mActive.end())
        return;

    std::shared_ptr<ServerTransport> transport = active->second.lock();
    if (transport != nullptr)
        transport->markClosed(reason);
    mActive.erase(active);
}

void BedrockListener::onReceiveIPSupport(RakPeerHelper::IPSupport support) {
    (void) support;
}

void BedrockListener::_run() {
    while (mRunning.load()) {
        for (std::unique_ptr<Connector> &connector: mConnectors)
            connector->runEvents();

        _tickLogins();
        _updatePlayerCount();
        std::this_thread::sleep_for(std::chrono::milliseconds(LISTENER_IDLE_WAIT_MS));
    }
}

void BedrockListener::_updatePlayerCount() {
    int count = 0;
    for (auto &entry: mActive) {
        std::shared_ptr<ServerTransport> transport = entry.second.lock();
        if (transport != nullptr && transport->isConnected())
            count++;
    }
    mPlayerCount.store(count);

    if (mSessionHost == nullptr)
        return;

    std::string serverName = mSettings.mServerName;
    std::string subName = mSettings.mSubName;
    int shownCount = count;
    int maxPlayers = mSettings.mMaxPlayers;
    {
        std::lock_guard<std::mutex> guard(mAdvertisementMutex);
        if (!mAdvertisedServerName.empty())
            serverName = mAdvertisedServerName;
        if (!mAdvertisedSubName.empty())
            subName = mAdvertisedSubName;
        if (mAdvertisedPlayerCount >= 0)
            shownCount = mAdvertisedPlayerCount;
        if (mAdvertisedMaxPlayers > 0)
            maxPlayers = mAdvertisedMaxPlayers;
    }

    const bool changed = mAdvertisementChanged.exchange(false);
    if (!changed && shownCount == mPublishedPlayerCount)
        return;

    HostedWorld world;
    world.mWorldName = serverName;
    world.mHostName = subName;
    world.mVersion = mSettings.mGameVersion;
    world.mProtocol = mSettings.mProtocolVersion;
    world.mMemberCount = shownCount;
    world.mMaxMemberCount = maxPlayers;
    world.mSignalingType = mSettings.mOnlineSignaling;
    world.mNetherNetId = mNetherNetId;
    world.mPlayerMessagingId = mPlayerMessagingId;
    mSessionHost->update(world);
    mPublishedPlayerCount = shownCount;
}

bool BedrockListener::isOnline(std::string &outReason) const {
    if (!mRunning.load()) {
        outReason = "the listener is not running";
        return false;
    }
    if (!mSettings.mNetherNet || mSettings.mAuthentication == nullptr)
        return true;

    if (mSignaling == nullptr) {
        outReason = "the NetherNet signaling is not connected";
        return false;
    }
    if (mSignaling->isClosed()) {
        outReason = "the NetherNet signaling closed: " + mSignaling->getCloseReason();
        return false;
    }
    if (!mSettings.mPublishSession)
        return true;

    if (mSessionHost == nullptr || !mSessionHost->isPublished()) {
        outReason = "the multiplayer session is not published";
        return false;
    }
    if (mSessionHost->getFailedRecoveries() >= MAX_FAILED_SESSION_RECOVERIES) {
        outReason = "the multiplayer session could not be restored";
        return false;
    }
    return true;
}

void BedrockListener::advertise(const std::string &serverName, const std::string &subName, int playerCount,
                                int maxPlayers) {
    std::lock_guard<std::mutex> guard(mAdvertisementMutex);
    mAdvertisedServerName = serverName;
    mAdvertisedSubName = subName;
    mAdvertisedPlayerCount = playerCount;
    mAdvertisedMaxPlayers = maxPlayers;
    mAdvertisementChanged.store(true);
}

void BedrockListener::_tickLogins() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<NetworkIdentifier> finished;

    for (auto &entry: mPending) {
        PendingLogin &login = *entry.second;
        BedrockConnection &connection = *login.mIncoming.mConnection;

        connection.update();

        if (mSettings.mDeferResourcePacks && login.mState == LoginState::WaitingResourcePacks) {
            finished.push_back(entry.first);
            continue;
        }

        std::string payload;
        bool keep = true;
        while (keep && !connection.isClosed() && connection.receiveRaw(payload))
            keep = _handleLoginPacket(login, std::move(payload));

        connection.flush();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - login.mStarted).count();
        if (keep && !connection.isClosed() && elapsed >= (long long) mSettings.mLoginTimeoutMs) {
            connection.disconnect("disconnectionScreen.timeout");
            keep = false;
        }

        if (!keep || connection.isClosed())
            finished.push_back(entry.first);
    }

    for (const NetworkIdentifier &id: finished) {
        auto it = mPending.find(id);
        if (it == mPending.end())
            continue;

        PendingLogin &login = *it->second;
        if (!login.mIncoming.mConnection->isClosed() && login.mState == LoginState::WaitingResourcePacks) {
            mActive[id] = login.mTransport;

            std::lock_guard<std::mutex> guard(mAcceptMutex);
            mAccepted.push_back(std::move(login.mIncoming));
            mAcceptCondition.notify_one();
        }

        mPending.erase(it);
    }
}

bool BedrockListener::_handleLoginPacket(PendingLogin &login, std::string payload) {
    MinecraftPacketIds id;
    if (!BedrockConnection::peekPacketId(payload, id))
        return true;

    switch (login.mState) {
        case LoginState::WaitingNetworkSettings:
            if (id == MinecraftPacketIds::RequestNetworkSettings)
                return _handleNetworkSettings(login, std::move(payload));
            return true;

        case LoginState::WaitingLogin:
            if (id == MinecraftPacketIds::Login)
                return _handleLogin(login, std::move(payload));
            return true;

        case LoginState::WaitingHandshake:
            if (id == MinecraftPacketIds::ClientToServerHandshake)
                _completeLogin(login);
            return !(mSettings.mDeferResourcePacks && login.mState == LoginState::WaitingResourcePacks);

        case LoginState::WaitingResourcePacks: {
            if (id != MinecraftPacketIds::ResourcePackClientResponse)
                return true;

            std::shared_ptr<ResourcePackClientResponsePacket> response =
                    std::dynamic_pointer_cast<ResourcePackClientResponsePacket>(
                            login.mIncoming.mConnection->decode(std::move(payload)));
            if (response == nullptr)
                return true;

            if (response->mStatus == ResourcePackClientResponsePacket::Status::Completed)
                return false;

            _finishLogin(login);
            return true;
        }
    }

    return true;
}

bool BedrockListener::_handleNetworkSettings(PendingLogin &login, std::string payload) {
    BedrockConnection &connection = *login.mIncoming.mConnection;
    std::shared_ptr<RequestNetworkSettingsPacket> request =
            std::dynamic_pointer_cast<RequestNetworkSettingsPacket>(connection.decode(std::move(payload)));
    if (request == nullptr)
        return true;

    login.mIncoming.mProtocolVersion = request->mProtocolVersion;

    if (mSettings.mProtocolVersion > 0 && request->mProtocolVersion != mSettings.mProtocolVersion) {
        PlayStatusPacket status;
        status.mStatus = request->mProtocolVersion < mSettings.mProtocolVersion
                         ? PlayStatusPacket::Status::LoginFailedClientOld
                         : PlayStatusPacket::Status::LoginFailedServerOld;
        connection.send(status);
        connection.flush();
        connection.close("protocol mismatch");
        return false;
    }

    NetworkSettingsPacket settings;
    settings.mCompressionThreshold = mSettings.mCompressionThreshold;
    settings.mCompressionAlgorithm = NetworkSettingsPacket::CompressionAlgorithm::ZLib;
    settings.mClientThrottleEnabled = false;
    settings.mClientThrottleThreshold = 0;
    settings.mClientThrottleScalar = 0.0f;
    connection.send(settings);
    connection.flush();
    connection.enableCompression(CompressedNetworkPeer::CompressionAlgorithm::ZLib, mSettings.mCompressionThreshold);

    login.mState = LoginState::WaitingLogin;
    return true;
}

bool BedrockListener::_handleLogin(PendingLogin &login, std::string payload) {
    BedrockConnection &connection = *login.mIncoming.mConnection;
    std::shared_ptr<LoginPacket> packet = std::dynamic_pointer_cast<LoginPacket>(connection.decode(std::move(payload)));
    if (packet == nullptr)
        return true;

    ConnectionRequest request;
    if (!request.parse(packet->mAuthJwt, packet->mClientJwt)) {
        connection.disconnect("disconnectionScreen.notAuthenticated");
        return false;
    }

    IncomingConnection &incoming = login.mIncoming;
    incoming.mIdentity.mDisplayName = request.getDisplayName();
    incoming.mIdentity.mIdentity = request.getIdentity();
    incoming.mIdentity.mXuid = request.getXuid();
    incoming.mIdentity.mTitleId = request.getTitleId();
    incoming.mLanguageCode = request.getLanguageCode();
    incoming.mDeviceOS = request.getBuildPlatform();
    incoming.mGameVersion = ConnectionRequest::findJsonString(
            ConnectionRequest::readJwtPayload(packet->mClientJwt), GAME_VERSION_KEY);
    incoming.mAuthJwt = packet->mAuthJwt;
    incoming.mClientJwt = packet->mClientJwt;

    if (login.mRequiresNonce) {
        const std::string nonce = ConnectionRequest::findJsonString(
                ConnectionRequest::readJwtPayload(packet->mClientJwt), NONCE_KEY);
        if (mSessionHost == nullptr || !mSessionHost->validateNonce(request.getXuid(), nonce)) {
            connection.disconnect("disconnectionScreen.notAuthenticated");
            return false;
        }
    }

    if (!mSettings.mEncryption) {
        _completeLogin(login);
        return !mSettings.mDeferResourcePacks;
    }

    Jwt::Token token;
    const std::string clientPublicKey = Jwt::parse(packet->mClientJwt, token) ? Jwt::readX5u(token) : std::string();
    const std::shared_ptr<KeyPair> serverKey = KeyPair::generate();

    if (clientPublicKey.empty() || serverKey == nullptr
        || !connection.startServerEncryption(*serverKey, clientPublicKey)) {
        connection.disconnect("disconnectionScreen.notAuthenticated");
        return false;
    }

    login.mState = LoginState::WaitingHandshake;
    return true;
}

void BedrockListener::_completeLogin(PendingLogin &login) {
    BedrockConnection &connection = *login.mIncoming.mConnection;

    PlayStatusPacket status;
    status.mStatus = PlayStatusPacket::Status::LoginSuccess;
    connection.send(status);

    if (mSettings.mDeferResourcePacks) {
        connection.flush();
        login.mState = LoginState::WaitingResourcePacks;
        return;
    }

    ResourcePacksInfoPacket packs;
    packs.mForcedToAccept = false;
    packs.mHasAddonPacks = false;
    packs.mScriptingEnabled = false;
    packs.mVibrantVisualsForceDisabled = false;
    packs.mWorldTemplateVersion = "";
    connection.send(packs);
    connection.flush();

    login.mState = LoginState::WaitingResourcePacks;
}

void BedrockListener::_finishLogin(PendingLogin &login) {
    BedrockConnection &connection = *login.mIncoming.mConnection;

    ResourcePackStackPacket stack;
    stack.mForcedToAccept = false;
    stack.mGameVersion = mSettings.mGameVersion;
    stack.mExperimentsPreviouslyToggled = false;
    stack.mHasEditorPacks = false;
    connection.send(stack);
    connection.flush();
}

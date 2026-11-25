#pragma once

#include "Network/BedrockConnection.h"
#include "Network/Client/ClientConnectionRequest.h"
#include "Network/NetworkEnums.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class MinecraftAuthentication;

class PacketCodecContext;

namespace nethernet {
    class Signaling;
}

enum class NetherNetSignalingType : int {
    Lan = 0,
    WebSocket = 1,
    JsonRpc = 2,
    Http = 3
};

struct NetherNetTarget {
    std::string mNetworkId;
    NetherNetSignalingType mSignalingType = NetherNetSignalingType::Lan;
    std::shared_ptr<nethernet::Signaling> mSignaling;
    bool mAllowIdentitylessServer = false;
    bool mDisableTrickleIce = false;
};

struct ResourcePackOffer {
    std::string mPackId;
    std::string mPackVersion;
    uint64_t mPackSize = 0;
    std::string mContentKey;
    std::string mSubPackName;
    std::string mCdnUrl;
    bool mRequired = false;
};

struct DownloadedResourcePack {
    ResourcePackOffer mOffer;
    std::string mData;
};

enum class ResourcePackDecision : int {
    Pending = 0,
    Download = 1,
    Skip = 2
};

struct ResourcePackCallbacks {
    std::function<bool(const DownloadedResourcePack &, std::string &)> mValidate;
    std::function<bool(const ResourcePackOffer &)> mIsCached;
    std::function<void(const std::vector<ResourcePackOffer> &)> mOffer;
    std::function<ResourcePackDecision()> mDecision;
    std::function<void(uint64_t, uint64_t)> mProgress;
    std::function<bool(MinecraftPacketIds, const std::string &, BedrockConnection &, std::string &)> mRelayPacket;
    std::function<bool(BedrockConnection &, bool &, bool &, std::string &)> mRelayTick;
    unsigned int mRelayTimeoutMs = 180000;
};

struct ClientConnectionSettings {
    std::string mHost;
    unsigned short mPort = 19132;
    int mProtocolVersion = 0;
    std::string mGameVersion;
    ClientIdentityData mIdentity;
    ClientData mClientData;
    MinecraftAuthentication *mAuthentication = nullptr;
    bool mLegacyAuthentication = false;
    bool mKeepXboxIdentityData = false;
    bool mEnableClientCache = false;
    int mChunkRadius = 16;
    unsigned int mTimeoutMs = 30000;
    const std::atomic<bool> *mCancel = nullptr;
    const PacketCodecContext *mCodecContext = nullptr;
    TransportLayer mTransportLayer = TransportLayer::RakNet;
    NetherNetTarget mNetherNet;
    bool mDeferSpawn = false;
    bool mDeferGameData = false;
    ResourcePackCallbacks mResourcePacks;
    std::function<void(MinecraftPacketIds)> mPacketObserver;
    // May be called from the CDN worker; the receiver must be thread-safe.
    std::function<void(const std::string &)> mDiagnostic;
};

struct ClientConnectionResult {
    std::unique_ptr<BedrockConnection> mConnection;
    ClientIdentityData mIdentity;
    ClientData mClientData;
    std::vector<DownloadedResourcePack> mResourcePacks;
    std::vector<ResourcePackOffer> mOfferedPacks;
    std::string mError;
};

class ClientNetworkSystem {
public:
    static ClientConnectionResult dial(const ClientConnectionSettings &settings);
};

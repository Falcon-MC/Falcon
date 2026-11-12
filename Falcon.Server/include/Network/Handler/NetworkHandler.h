#pragma once

#include "Core/Threading/TaskQueue.h"
#include "Network/BatchedNetworkPeer.h"
#include "Network/CompressedNetworkPeer.h"
#include "Network/Connector.h"
#include "Network/EncryptedNetworkPeer.h"
#include "Network/SharedPacket.h"
#include "Protocol/MinecraftPacketIds.h"
#include "Server/Profiler.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Packet;
class PacketCodecContext;

class NetworkHandler : public Connector::ConnectionCallbacks {
public:
    class Connection {
    public:
        Connection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer);

        const NetworkIdentifier &getId() const {
            return mId;
        }

        NetworkPeer *getPeer() const {
            return mPeer.get();
        }

        EncryptedNetworkPeer *getEncryptedPeer() const {
            return mEncryptedPeer.get();
        }

        CompressedNetworkPeer *getCompressedPeer() const {
            return mCompressedPeer.get();
        }

        BatchedNetworkPeer *getBatchedPeer() const {
            return mBatchedPeer.get();
        }

        bool isFailureReported() const {
            return mFailureReported;
        }

        void markFailureReported() {
            mFailureReported = true;
        }

    private:
        NetworkIdentifier mId;
        std::shared_ptr<NetworkPeer> mPeer;
        std::shared_ptr<EncryptedNetworkPeer> mEncryptedPeer;
        std::shared_ptr<CompressedNetworkPeer> mCompressedPeer;
        std::shared_ptr<BatchedNetworkPeer> mBatchedPeer;
        bool mFailureReported = false;
    };

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual bool onValidateIncomingConnection(const NetworkIdentifier &) {
            return true;
        }

        virtual void onNewIncomingConnection(const NetworkIdentifier &) {}

        virtual void onNewIncomingConnection(const NetworkIdentifier &id, uint16_t mtuSize) {
            (void) mtuSize;
            onNewIncomingConnection(id);
        }

        virtual void onConnectionClosed(const NetworkIdentifier &, DisconnectFailReason, const std::string &) {}

        virtual void onDataReceived(const NetworkIdentifier &, const std::string &) {}

        virtual void onConnectionFailed(const NetworkIdentifier &) {}
    };

    struct InboundEvent {
        enum class Kind : int { Data = 0, Opened = 1, Closed = 2, Failed = 3 };

        Kind mKind = Kind::Data;
        NetworkIdentifier mId;
        std::string mData;
        DisconnectFailReason mReason = DisconnectFailReason::Unknown;
        uint16_t mMtuSize = 0;
    };

    struct OutboundCommand {
        enum class Kind : int {
            Data = 0,
            Flush = 1,
            EnableCompression = 2,
            EnableEncryption = 3,
            Close = 4,
            Adopt = 5,
            Remove = 6
        };

        Kind mKind = Kind::Data;
        NetworkIdentifier mId;
        std::string mData;
        bool mBroadcast = false;
        NetworkPeer::Reliability mReliability = NetworkPeer::Reliability::ReliableOrdered;
        Compressibility mCompressibility = Compressibility::Compressible;
        CompressedNetworkPeer::CompressionAlgorithm mAlgorithm = CompressedNetworkPeer::CompressionAlgorithm::ZLib;
        unsigned short mThreshold = 0;
        EncryptionKey mEncryptionKey{};
        DisconnectFailReason mReason = DisconnectFailReason::Unknown;
        uint16_t mMtuSize = 0;
        std::unique_ptr<Connection> mConnection;
        // sent in place of mData: one encoding and one compression for every recipient
        std::shared_ptr<const SharedPacket> mShared;
        // released once the shard has taken the data
        std::shared_ptr<std::atomic<size_t>> mPendingBytes;
    };

    using OutboundFilter = std::function<bool(const NetworkIdentifier &, std::string &)>;

    // every send from the game thread, before encoding; false drops it for all its recipients
    using SendHook = std::function<bool(const std::vector<NetworkIdentifier> &, MinecraftPacketIds)>;

    // nullptr: keep the caller's context
    using ContextResolver = std::function<const PacketCodecContext *(const NetworkIdentifier &)>;

    explicit NetworkHandler(std::unique_ptr<Connector> connector);

    ~NetworkHandler() override;

    bool host(const ConnectionDefinition &definition);

    void disconnect();

    void startIoThread();

    void stopIoThread();

    bool isIoThreadRunning() const {
        return mIoRunning.load();
    }

    void runEvents();

    // between ticks: handles the connections admit lets through now; the others wait for runEvents in order
    void runEventsFor(const std::function<bool(const NetworkIdentifier &)> &admit);

    size_t getPendingOutboundCount() const;

    // not yet batched by the IO shard: lets bulk senders hold back; main thread only, like every send
    size_t getPendingOutboundBytes(const NetworkIdentifier &id) const;

    size_t getPendingInboundCount() const {
        return mInbound.size();
    }

    void send(const NetworkIdentifier &id, const Packet &packet, const PacketCodecContext &context);

    // encodes once per codec context, queues every copy under one lock
    void sendToMany(const std::vector<NetworkIdentifier> &ids, const Packet &packet, const PacketCodecContext &context);

    void send(const NetworkIdentifier &id, const std::string &data,
              NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered,
              Compressibility compressibility = Compressibility::Compressible);

    // the packet bytes as written with a header, already encoded for this session's codec
    void sendShared(const NetworkIdentifier &id, const std::shared_ptr<const SharedPacket> &packet,
                    NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered);

    void sendToAll(const std::string &data,
                   NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered,
                   Compressibility compressibility = Compressibility::Compressible);

    // needed before the connection settings change
    void flush(const NetworkIdentifier &id);

    void enableCompression(const NetworkIdentifier &id, CompressedNetworkPeer::CompressionAlgorithm algorithm,
                           unsigned short threshold);

    void enableEncryption(const NetworkIdentifier &id, const EncryptionKey &key);

    // sends nothing, then reports the connection closed
    void closeConnection(const NetworkIdentifier &id);

    size_t getConnectionCount() const {
        return mConnectionCount.load();
    }

    // the transport's average round trip in milliseconds, refreshed about once a second; -1 when unknown
    int getAveragePing(const NetworkIdentifier &id) const;

    // how the client compressed its first batches, refreshed about once a second; false when unknown
    bool getBatchTraits(const NetworkIdentifier &id, CompressedNetworkPeer::BatchTraits &outTraits) const;

    Connector &getConnector() {
        return *mConnectors.front();
    }

    void addConnector(std::unique_ptr<Connector> connector);

    void setProfiler(Profiler *profiler) {
        mProfiler = profiler;
    }

    void setOutboundFilter(OutboundFilter filter);

    void setSendHook(SendHook hook);

    // callers keep passing the server's context whatever version the client speaks
    void setContextResolver(ContextResolver resolver);

    void addListener(Listener *listener);

    void removeListener(Listener *listener);

    // a session served in process (a gametest simulated player): nothing sent to it leaves the server
    void addLocalSession(const NetworkIdentifier &id);

    void removeLocalSession(const NetworkIdentifier &id);

    bool isLocalSession(const NetworkIdentifier &id) const;

private:
    bool onValidateIncomingConnection(const NetworkIdentifier &id) override;

    void onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) override;

    void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                            const std::string &message) override;

    static NetworkPeer::Reliability _toPeerReliability(const Packet &packet);

    static Compressibility _toPeerCompressibility(const Packet &packet);

    // a connection always maps to one shard: its order is kept while compression spreads over cores
    struct Shard {
        TaskQueue<OutboundCommand> mOutbound;
        std::deque<OutboundCommand> mBacklog;
        std::unordered_map<NetworkIdentifier, std::unique_ptr<Connection>, NetworkIdentifier::Hasher> mConnections;
        std::thread mThread;
        // written by the shard's io thread, read by the game thread
        mutable std::mutex mPingMutex;
        std::unordered_map<NetworkIdentifier, int, NetworkIdentifier::Hasher> mPings;
        std::unordered_map<NetworkIdentifier, CompressedNetworkPeer::BatchTraits, NetworkIdentifier::Hasher>
                mBatchTraits;
        std::chrono::steady_clock::time_point mPingRefresh;
    };

    Shard &_shardFor(const NetworkIdentifier &id);

    void _pushOutbound(OutboundCommand command);

    void _queueData(const NetworkIdentifier &id, const std::string &data, NetworkPeer::Reliability reliability,
                    Compressibility compressibility);

    // false: the hook dropped the packet these bytes start with
    bool _allowRaw(const std::vector<NetworkIdentifier> &ids, const std::string &data);

    void _trackPending(OutboundCommand &command);

    static size_t _pendingSize(const OutboundCommand &command);

    void _ioLoop(size_t shardIndex);

    void _pumpIo();

    void _pumpControl();

    void _pumpShard(Shard &shard, bool bounded);

    void _applyOutbound(Shard &shard, OutboundCommand &command);

    // once per connection: the game drops it
    void _reportFailure(Connection &connection);

    void _dispatch(InboundEvent &event);

    Profiler *mProfiler = nullptr;
    std::vector<std::unique_ptr<Connector>> mConnectors;
    std::vector<std::unique_ptr<Shard>> mShards;
    // only the control pump touches it
    std::unordered_set<NetworkIdentifier, NetworkIdentifier::Hasher> mLive;
    // dropped by a shard on server request; the control pump closes them in their connector
    TaskQueue<NetworkIdentifier> mServerClosed;
    std::vector<Listener *> mListeners;
    OutboundFilter mOutboundFilter;
    SendHook mSendHook;
    ContextResolver mContextResolver;
    mutable std::mutex mLocalSessionsMutex;
    std::unordered_set<NetworkIdentifier, NetworkIdentifier::Hasher> mLocalSessions;
    std::atomic<size_t> mLocalSessionCount{0};

    TaskQueue<InboundEvent> mInbound;
    // held back by runEventsFor; a connection with one held keeps all its later events behind it
    std::vector<InboundEvent> mDeferred;
    std::unordered_set<NetworkIdentifier, NetworkIdentifier::Hasher> mDeferredIds;
    // main thread view: from the Opened event to the Closed one
    std::unordered_map<NetworkIdentifier, std::shared_ptr<std::atomic<size_t>>, NetworkIdentifier::Hasher>
            mPendingBytes;

    std::atomic<bool> mIoRunning;
    std::atomic<size_t> mConnectionCount;
};

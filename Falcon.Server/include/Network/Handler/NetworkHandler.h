#pragma once

#include "Core/Threading/TaskQueue.h"
#include "Network/BatchedNetworkPeer.h"
#include "Network/CompressedNetworkPeer.h"
#include "Network/Connector.h"
#include "Network/EncryptedNetworkPeer.h"
#include "Server/Profiler.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
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

        const NetworkIdentifier &getId() const { return mId; }

        NetworkPeer *getPeer() const { return mPeer.get(); }

        EncryptedNetworkPeer *getEncryptedPeer() const {
            return mEncryptedPeer.get();
        }

        CompressedNetworkPeer *getCompressedPeer() const { return mCompressedPeer.get(); }

        BatchedNetworkPeer *getBatchedPeer() const { return mBatchedPeer.get(); }

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

        virtual bool onValidateIncomingConnection(const NetworkIdentifier &) { return true; }

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
        enum class Kind : int {
            Data = 0,
            Opened = 1,
            Closed = 2,
            Failed = 3
        };

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
        // The connection's count of queued bytes, released once its shard has taken the data.
        std::shared_ptr<std::atomic<size_t>> mPendingBytes;
    };

    using OutboundFilter = std::function<bool(const NetworkIdentifier &, std::string &)>;

    /**
     * Returns the codec context of the protocol a connection chose, or nullptr to keep the one the caller passed.
     */
    using ContextResolver = std::function<const PacketCodecContext *(const NetworkIdentifier &)>;

    explicit NetworkHandler(std::unique_ptr<Connector> connector);

    ~NetworkHandler() override;

    bool host(const ConnectionDefinition &definition);

    void disconnect();

    void startIoThread();

    void stopIoThread();

    bool isIoThreadRunning() const { return mIoRunning.load(); }

    void runEvents();

    size_t getPendingOutboundCount() const;

    /**
     * Bytes sent to a connection that its IO shard has not batched yet, so a sender can hold back bulk data
     * instead of queueing it faster than it is compressed. Main thread only, like every send.
     */
    size_t getPendingOutboundBytes(const NetworkIdentifier &id) const;

    size_t getPendingInboundCount() const { return mInbound.size(); }

    void send(const NetworkIdentifier &id, const Packet &packet, const PacketCodecContext &context);

    /**
     * Sends one packet to several connections, encoding it once per codec context and queueing every copy under a
     * single lock.
     */
    void sendToMany(const std::vector<NetworkIdentifier> &ids, const Packet &packet, const PacketCodecContext &context);

    void send(const NetworkIdentifier &id, const std::string &data,
              NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered,
              Compressibility compressibility = Compressibility::Compressible);

    void sendToAll(const std::string &data,
                   NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered,
                   Compressibility compressibility = Compressibility::Compressible);

    // Flushes the pending batch immediately, needed before the connection settings change
    void flush(const NetworkIdentifier &id);

    void enableCompression(const NetworkIdentifier &id, CompressedNetworkPeer::CompressionAlgorithm algorithm,
                           unsigned short threshold);

    void enableEncryption(const NetworkIdentifier &id, const EncryptionKey &key);

    /**
     * Drops a connection without sending it anything, then reports it as closed.
     */
    void closeConnection(const NetworkIdentifier &id);

    size_t getConnectionCount() const { return mConnectionCount.load(); }

    Connector &getConnector() { return *mConnectors.front(); }

    void addConnector(std::unique_ptr<Connector> connector);

    void setProfiler(Profiler *profiler) { mProfiler = profiler; }

    void setOutboundFilter(OutboundFilter filter);

    /**
     * Makes every packet sent to a connection use the codec of its protocol, so callers keep passing the
     * server's context whatever version the client speaks.
     */
    void setContextResolver(ContextResolver resolver);

    void addListener(Listener *listener);

    void removeListener(Listener *listener);

private:
    bool onValidateIncomingConnection(const NetworkIdentifier &id) override;

    void onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) override;

    void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                            const std::string &message) override;

    static NetworkPeer::Reliability _toPeerReliability(const Packet &packet);

    static Compressibility _toPeerCompressibility(const Packet &packet);

    /**
     * One IO thread's share of the connections. A connection always maps to the same shard, which alone
     * batches, compresses and sends its packets and reads what it received, so its order is kept while
     * the compression of several connections runs on several cores.
     */
    struct Shard {
        TaskQueue<OutboundCommand> mOutbound;
        std::deque<OutboundCommand> mBacklog;
        std::unordered_map<NetworkIdentifier, std::unique_ptr<Connection>, NetworkIdentifier::Hasher> mConnections;
        std::thread mThread;
    };

    Shard &_shardFor(const NetworkIdentifier &id);

    void _pushOutbound(OutboundCommand command);

    void _trackPending(OutboundCommand &command);

    void _ioLoop(size_t shardIndex);

    void _pumpIo();

    void _pumpControl();

    void _pumpShard(Shard &shard, bool bounded);

    void _applyOutbound(Shard &shard, OutboundCommand &command);

    // Reports a connection whose transport failed, once, so the game drops it.
    void _reportFailure(Connection &connection);

    Profiler *mProfiler = nullptr;
    std::vector<std::unique_ptr<Connector>> mConnectors;
    std::vector<std::unique_ptr<Shard>> mShards;
    // Connections the connectors reported open and not closed yet; only the control pump touches it.
    std::unordered_set<NetworkIdentifier, NetworkIdentifier::Hasher> mLive;
    // Connections a shard dropped on the server's request, for the control pump to close in their connector.
    TaskQueue<NetworkIdentifier> mServerClosed;
    std::vector<Listener *> mListeners;
    OutboundFilter mOutboundFilter;
    ContextResolver mContextResolver;

    TaskQueue<InboundEvent> mInbound;
    // Open connections as the main thread knows them, from their Opened event to their Closed one.
    std::unordered_map<NetworkIdentifier, std::shared_ptr<std::atomic<size_t>>, NetworkIdentifier::Hasher> mPendingBytes;

    std::atomic<bool> mIoRunning;
    std::atomic<size_t> mConnectionCount;
};

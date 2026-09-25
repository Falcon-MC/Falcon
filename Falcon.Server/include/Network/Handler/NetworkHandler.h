#pragma once

#include "Core/Concurrency/TaskQueue.h"
#include "Network/BatchedNetworkPeer.h"
#include "Network/CompressedNetworkPeer.h"
#include "Network/Connector.h"
#include "Network/EncryptedNetworkPeer.h"
#include "Server/Profiler.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
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
    };

    struct OutboundCommand {
        enum class Kind : int {
            Data = 0,
            Flush = 1,
            EnableCompression = 2,
            EnableEncryption = 3
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
    };

    using OutboundFilter = std::function<bool(const NetworkIdentifier &, std::string &)>;

    explicit NetworkHandler(std::unique_ptr<Connector> connector);

    ~NetworkHandler() override;

    bool host(const ConnectionDefinition &definition);

    void disconnect();

    void startIoThread();

    void stopIoThread();

    bool isIoThreadRunning() const { return mIoRunning.load(); }

    void runEvents();

    size_t getPendingOutboundCount() const { return mOutbound.size(); }

    size_t getPendingInboundCount() const { return mInbound.size(); }

    void send(const NetworkIdentifier &id, const Packet &packet, const PacketCodecContext &context);

    void sendToAll(const Packet &packet, const PacketCodecContext &context);

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

    size_t getConnectionCount() const { return mConnectionCount.load(); }

    Connector &getConnector() { return *mConnectors.front(); }

    void addConnector(std::unique_ptr<Connector> connector);

    void setProfiler(Profiler *profiler) { mProfiler = profiler; }

    void setOutboundFilter(OutboundFilter filter);

    void addListener(Listener *listener);

    void removeListener(Listener *listener);

private:
    bool onValidateIncomingConnection(const NetworkIdentifier &id) override;

    void onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) override;

    void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                            const std::string &message) override;

    static NetworkPeer::Reliability _toPeerReliability(const Packet &packet);

    static Compressibility _toPeerCompressibility(const Packet &packet);

    Connection *getConnection(const NetworkIdentifier &id);

    void _ioLoop();

    void _pumpIo();

    void _applyOutbound(OutboundCommand &command);

    Profiler *mProfiler = nullptr;
    std::vector<std::unique_ptr<Connector>> mConnectors;
    std::unordered_map<NetworkIdentifier, std::unique_ptr<Connection>, NetworkIdentifier::Hasher> mConnections;
    std::vector<Listener *> mListeners;
    OutboundFilter mOutboundFilter;

    TaskQueue<InboundEvent> mInbound;
    TaskQueue<OutboundCommand> mOutbound;

    std::thread mIoThread;
    std::atomic<bool> mIoRunning;
    std::atomic<size_t> mConnectionCount;
};

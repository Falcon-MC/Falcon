#include "Network/Handler/NetworkHandler.h"

#include "Core/Utility/BinaryStream.h"
#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/Packet.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace {
    const unsigned MAX_IO_SHARDS = 8;

    /**
     * Compressible bytes a shard hands to its connections before it reads what they received again. Zlib
     * compresses a few tens of megabytes a second, so a burst of chunks is spread over many short passes
     * instead of one that leaves the transport's receive queues full and stalls every connection.
     */
    const size_t MAX_OUTBOUND_BYTES_PER_PASS = 256 * 1024;
    const std::chrono::milliseconds MAX_OUTBOUND_TIME_PER_PASS(4);

    size_t ioShardCount() {
        const unsigned cores = std::thread::hardware_concurrency();
        return (size_t) std::max(1u, std::min(MAX_IO_SHARDS, cores / 2));
    }
}

NetworkHandler::Connection::Connection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer)
        : mId(id), mPeer(std::move(peer)) {
    mEncryptedPeer = std::make_shared<EncryptedNetworkPeer>(mPeer);
    mCompressedPeer = std::make_shared<CompressedNetworkPeer>(mEncryptedPeer);
    mBatchedPeer = std::make_shared<BatchedNetworkPeer>(mCompressedPeer);
}

NetworkHandler::NetworkHandler(std::unique_ptr<Connector> connector)
        : mIoRunning(false), mConnectionCount(0) {
    const size_t shards = ioShardCount();
    for (size_t index = 0; index < shards; ++index)
        mShards.push_back(std::unique_ptr<Shard>(new Shard()));

    addConnector(std::move(connector));
}

NetworkHandler::~NetworkHandler() {
    disconnect();

    for (std::unique_ptr<Connector> &connector: mConnectors)
        connector->setCallbacks(nullptr);
}

void NetworkHandler::addConnector(std::unique_ptr<Connector> connector) {
    if (connector == nullptr)
        return;

    connector->setCallbacks(this);
    mConnectors.push_back(std::move(connector));
}

bool NetworkHandler::host(const ConnectionDefinition &definition) {
    bool hosted = false;

    for (std::unique_ptr<Connector> &connector: mConnectors) {
        if (connector->host(definition))
            hosted = true;
    }

    return hosted;
}

void NetworkHandler::disconnect() {
    stopIoThread();

    for (std::unique_ptr<Connector> &connector: mConnectors)
        connector->disconnect();

    for (std::unique_ptr<Shard> &shard: mShards) {
        shard->mConnections.clear();
        shard->mBacklog.clear();
    }
    mLive.clear();
    mConnectionCount.store(0);
}

void NetworkHandler::startIoThread() {
    if (mIoRunning.load())
        return;

    mIoRunning.store(true);
    for (size_t index = 0; index < mShards.size(); ++index)
        mShards[index]->mThread = std::thread(&NetworkHandler::_ioLoop, this, index);
}

void NetworkHandler::stopIoThread() {
    if (!mIoRunning.load())
        return;

    mIoRunning.store(false);

    for (std::unique_ptr<Shard> &shard: mShards) {
        if (shard->mThread.joinable())
            shard->mThread.join();
    }

    // A shard that stopped after the control shard may still have queued transport closes.
    _pumpControl();

    for (std::unique_ptr<Shard> &shard: mShards)
        shard->mOutbound.close();
    mServerClosed.close();
    mInbound.close();
}

void NetworkHandler::_ioLoop(size_t shardIndex) {
    Shard &shard = *mShards[shardIndex];
    // The connectors are not thread safe, so only the first shard runs them.
    const bool control = shardIndex == 0;

    while (mIoRunning.load()) {
        if (control)
            _pumpControl();
        _pumpShard(shard, true);
        if (shard.mBacklog.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (control)
        _pumpControl();
    _pumpShard(shard, false);
}

void NetworkHandler::_pumpIo() {
    _pumpControl();
    for (std::unique_ptr<Shard> &shard: mShards)
        _pumpShard(*shard, false);
}

size_t NetworkHandler::getPendingOutboundCount() const {
    size_t pending = 0;
    for (const std::unique_ptr<Shard> &shard: mShards)
        pending += shard->mOutbound.size();
    return pending;
}

NetworkHandler::Shard &NetworkHandler::_shardFor(const NetworkIdentifier &id) {
    return *mShards[NetworkIdentifier::Hasher()(id) % mShards.size()];
}

void NetworkHandler::_pushOutbound(OutboundCommand command) {
    _shardFor(command.mId).mOutbound.push(std::move(command));
}

size_t NetworkHandler::getPendingOutboundBytes(const NetworkIdentifier &id) const {
    auto it = mPendingBytes.find(id);
    return it == mPendingBytes.end() ? 0 : it->second->load(std::memory_order_relaxed);
}

void NetworkHandler::_trackPending(OutboundCommand &command) {
    auto it = mPendingBytes.find(command.mId);
    if (it == mPendingBytes.end())
        return;

    command.mPendingBytes = it->second;
    it->second->fetch_add(command.mData.size(), std::memory_order_relaxed);
}

void NetworkHandler::addListener(Listener *listener) {
    if (listener != nullptr && std::find(mListeners.begin(), mListeners.end(), listener) == mListeners.end())
        mListeners.push_back(listener);
}

void NetworkHandler::removeListener(Listener *listener) {
    mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), listener), mListeners.end());
}

void NetworkHandler::flush(const NetworkIdentifier &id) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Flush;
    command.mId = id;
    _pushOutbound(std::move(command));
}

void NetworkHandler::enableCompression(const NetworkIdentifier &id,
                                       CompressedNetworkPeer::CompressionAlgorithm algorithm,
                                       unsigned short threshold) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::EnableCompression;
    command.mId = id;
    command.mAlgorithm = algorithm;
    command.mThreshold = threshold;
    _pushOutbound(std::move(command));
}

void NetworkHandler::enableEncryption(const NetworkIdentifier &id, const EncryptionKey &key) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::EnableEncryption;
    command.mId = id;
    command.mEncryptionKey = key;
    _pushOutbound(std::move(command));
}

void NetworkHandler::closeConnection(const NetworkIdentifier &id) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Close;
    command.mId = id;
    _pushOutbound(std::move(command));
}

NetworkPeer::Reliability NetworkHandler::_toPeerReliability(const Packet &packet) {
    switch (packet.mReliability) {
        case Packet::Reliability::Reliable:
            return NetworkPeer::Reliability::Reliable;
        case Packet::Reliability::Unreliable:
            return NetworkPeer::Reliability::Unreliable;
        case Packet::Reliability::UnreliableSequenced:
            return NetworkPeer::Reliability::UnreliableSequenced;
        default:
            return NetworkPeer::Reliability::ReliableOrdered;
    }
}

Compressibility NetworkHandler::_toPeerCompressibility(const Packet &packet) {
    return packet.mCompressible == Packet::Compressibility::Incompressible
           ? Compressibility::Incompressible
           : Compressibility::Compressible;
}

void NetworkHandler::send(const NetworkIdentifier &id, const Packet &packet, const PacketCodecContext &context) {
    const PacketCodecContext *own = mContextResolver ? mContextResolver(id) : nullptr;
    const PacketCodecContext &session = own != nullptr ? *own : context;
    if (!session.getCodec().supports(packet.getId()))
        return;

    BinaryStream stream;
    packet.writeWithHeader(stream, session);

    if (!mOutboundFilter) {
        send(id, stream.getBuffer(), _toPeerReliability(packet), _toPeerCompressibility(packet));
        return;
    }

    std::string data = stream.getBuffer();
    if (!mOutboundFilter(id, data))
        return;

    send(id, data, _toPeerReliability(packet), _toPeerCompressibility(packet));
}

void NetworkHandler::sendToMany(const std::vector<NetworkIdentifier> &ids, const Packet &packet,
                                const PacketCodecContext &context) {
    // Sessions on the same protocol share a codec context, so most recipients reuse one encoding.
    std::vector<std::pair<const PacketCodecContext *, std::string>> encoded;
    std::vector<std::vector<OutboundCommand>> commands(mShards.size());

    for (const NetworkIdentifier &id: ids) {
        const PacketCodecContext *own = mContextResolver ? mContextResolver(id) : nullptr;
        const PacketCodecContext &session = own != nullptr ? *own : context;
        if (!session.getCodec().supports(packet.getId()))
            continue;

        const std::string *data = nullptr;
        for (const auto &entry: encoded) {
            if (entry.first == &session) {
                data = &entry.second;
                break;
            }
        }
        if (data == nullptr) {
            BinaryStream stream;
            packet.writeWithHeader(stream, session);
            encoded.emplace_back(&session, stream.getBuffer());
            data = &encoded.back().second;
        }

        OutboundCommand command;
        command.mKind = OutboundCommand::Kind::Data;
        command.mId = id;
        command.mData = *data;
        if (mOutboundFilter && !mOutboundFilter(id, command.mData))
            continue;

        command.mReliability = _toPeerReliability(packet);
        command.mCompressibility = _toPeerCompressibility(packet);
        _trackPending(command);
        commands[NetworkIdentifier::Hasher()(id) % mShards.size()].push_back(std::move(command));
    }

    for (size_t index = 0; index < mShards.size(); ++index)
        mShards[index]->mOutbound.pushAll(std::move(commands[index]));
}

void NetworkHandler::setOutboundFilter(OutboundFilter filter) {
    mOutboundFilter = std::move(filter);
}

void NetworkHandler::setContextResolver(ContextResolver resolver) {
    mContextResolver = std::move(resolver);
}

void NetworkHandler::send(const NetworkIdentifier &id, const std::string &data, NetworkPeer::Reliability reliability,
                          Compressibility compressibility) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Data;
    command.mId = id;
    command.mData = data;
    command.mReliability = reliability;
    command.mCompressibility = compressibility;
    _trackPending(command);
    _pushOutbound(std::move(command));
}

void NetworkHandler::sendToAll(const std::string &data, NetworkPeer::Reliability reliability,
                               Compressibility compressibility) {
    for (std::unique_ptr<Shard> &shard: mShards) {
        OutboundCommand command;
        command.mKind = OutboundCommand::Kind::Data;
        command.mData = data;
        command.mBroadcast = true;
        command.mReliability = reliability;
        command.mCompressibility = compressibility;
        shard->mOutbound.push(std::move(command));
    }
}

void NetworkHandler::_applyOutbound(Shard &shard, OutboundCommand &command) {
    if (command.mKind == OutboundCommand::Kind::Data) {
        if (command.mBroadcast) {
            for (auto &entry: shard.mConnections)
                entry.second->getBatchedPeer()->sendPacket(command.mData, command.mReliability,
                                                           command.mCompressibility);
            return;
        }

        auto it = shard.mConnections.find(command.mId);
        if (it != shard.mConnections.end())
            it->second->getBatchedPeer()->sendPacket(command.mData, command.mReliability,
                                                     command.mCompressibility);
        return;
    }

    if (command.mKind == OutboundCommand::Kind::Adopt) {
        shard.mConnections[command.mId] = std::move(command.mConnection);

        // Reported by the shard so the game sees the connection before any of its data, and anything it
        // sends in reply is queued behind the adoption.
        InboundEvent event;
        event.mKind = InboundEvent::Kind::Opened;
        event.mId = command.mId;
        event.mMtuSize = command.mMtuSize;
        mInbound.push(std::move(event));
        return;
    }

    auto it = shard.mConnections.find(command.mId);
    if (it == shard.mConnections.end())
        return;
    Connection *connection = it->second.get();

    if (command.mKind == OutboundCommand::Kind::Close || command.mKind == OutboundCommand::Kind::Remove) {
        shard.mConnections.erase(it);

        InboundEvent event;
        event.mKind = InboundEvent::Kind::Closed;
        event.mId = command.mId;
        if (command.mKind == OutboundCommand::Kind::Close) {
            event.mReason = DisconnectFailReason::ConnectionRefused;
            mServerClosed.push(command.mId);
        } else {
            event.mReason = command.mReason;
            event.mData = command.mData;
        }
        mInbound.push(std::move(event));
        return;
    }

    if (command.mKind == OutboundCommand::Kind::Flush)
        connection->getBatchedPeer()->flush();
    else if (command.mKind == OutboundCommand::Kind::EnableEncryption)
        connection->getEncryptedPeer()->enableEncryption(command.mEncryptionKey);
    else
        connection->getCompressedPeer()->enableCompression(command.mAlgorithm, command.mThreshold);
}

void NetworkHandler::_pumpControl() {
    for (std::unique_ptr<Connector> &connector: mConnectors)
        connector->runEvents();

    for (const NetworkIdentifier &id: mServerClosed.drain()) {
        mLive.erase(id);
        mConnectionCount.store(mLive.size());
        for (std::unique_ptr<Connector> &connector: mConnectors) {
            if (connector->closeConnection(id))
                break;
        }
    }
}

void NetworkHandler::_pumpShard(Shard &shard, bool bounded) {
    std::string data;

    // Reading comes first: a transport holds off its own threads while a connection's receive queue is full.
    for (auto &entry: shard.mConnections) {
        NetworkPeer *peer = entry.second->getBatchedPeer();
        bool failed = false;

        // A transport can throw for one connection (for example a data channel closing mid-send);
        // that connection is dropped instead of the exception ending the whole server.
        try {
            while (peer->receivePacket(data) == NetworkPeer::DataStatus::HasData) {
                InboundEvent event;
                event.mKind = InboundEvent::Kind::Data;
                event.mId = entry.first;
                event.mData = data;
                mInbound.push(std::move(event));
            }

            peer->update();
        } catch (const std::exception &) {
            failed = true;
        }

        if ((failed || entry.second->getEncryptedPeer()->hasFailed()) && !entry.second->isFailureReported()) {
            entry.second->markFailureReported();

            InboundEvent event;
            event.mKind = InboundEvent::Kind::Failed;
            event.mId = entry.first;
            mInbound.push(std::move(event));
        }
    }

    std::vector<OutboundCommand> commands = shard.mOutbound.drain();
    shard.mBacklog.insert(shard.mBacklog.end(), std::make_move_iterator(commands.begin()),
                          std::make_move_iterator(commands.end()));

    const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + MAX_OUTBOUND_TIME_PER_PASS;
    size_t bytes = 0;
    while (!shard.mBacklog.empty()) {
        if (bounded && (bytes >= MAX_OUTBOUND_BYTES_PER_PASS || std::chrono::steady_clock::now() >= deadline))
            break;

        OutboundCommand command = std::move(shard.mBacklog.front());
        shard.mBacklog.pop_front();
        if (command.mCompressibility == Compressibility::Compressible)
            bytes += command.mBroadcast ? command.mData.size() * shard.mConnections.size() : command.mData.size();

        if (command.mPendingBytes != nullptr)
            command.mPendingBytes->fetch_sub(command.mData.size(), std::memory_order_relaxed);

        try {
            _applyOutbound(shard, command);
        } catch (const std::exception &) {
            auto it = shard.mConnections.find(command.mId);
            if (it != shard.mConnections.end() && !it->second->isFailureReported()) {
                it->second->markFailureReported();

                InboundEvent event;
                event.mKind = InboundEvent::Kind::Failed;
                event.mId = command.mId;
                mInbound.push(std::move(event));
            }
        }
    }

    for (auto &entry: shard.mConnections) {
        try {
            entry.second->getBatchedPeer()->flush();
        } catch (const std::exception &) {
            if (!entry.second->isFailureReported()) {
                entry.second->markFailureReported();

                InboundEvent event;
                event.mKind = InboundEvent::Kind::Failed;
                event.mId = entry.first;
                mInbound.push(std::move(event));
            }
        }
    }
}

void NetworkHandler::runEvents() {
    const bool profiling = mProfiler != nullptr;

    if (!mIoRunning.load()) {
        ProfilerScopedSection pumpSection(*mProfiler, ProfilerSection::NetworkPumpIo, profiling);
        _pumpIo();
    }

    std::vector<InboundEvent> events;
    {
        ProfilerScopedSection drainSection(*mProfiler, ProfilerSection::NetworkDrain, profiling);
        events = mInbound.drain();
    }

    for (InboundEvent &event: events) {
        switch (event.mKind) {
            case InboundEvent::Kind::Opened: {
                ProfilerScopedSection section(*mProfiler, ProfilerSection::NetworkConnection, profiling);
                mPendingBytes[event.mId] = std::make_shared<std::atomic<size_t>>(0);
                for (Listener *listener: mListeners)
                    listener->onNewIncomingConnection(event.mId, event.mMtuSize);
                break;
            }

            case InboundEvent::Kind::Closed: {
                ProfilerScopedSection section(*mProfiler, ProfilerSection::NetworkConnection, profiling);
                for (Listener *listener: mListeners)
                    listener->onConnectionClosed(event.mId, event.mReason, event.mData);
                mPendingBytes.erase(event.mId);
                break;
            }

            case InboundEvent::Kind::Failed: {
                ProfilerScopedSection section(*mProfiler, ProfilerSection::NetworkConnection, profiling);
                for (Listener *listener: mListeners)
                    listener->onConnectionFailed(event.mId);
                break;
            }

            default:
                for (Listener *listener: mListeners)
                    listener->onDataReceived(event.mId, event.mData);
                break;
        }
    }
}

bool NetworkHandler::onValidateIncomingConnection(const NetworkIdentifier &id) {
    for (Listener *listener: mListeners) {
        if (!listener->onValidateIncomingConnection(id))
            return false;
    }

    return true;
}

void NetworkHandler::onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) {
    const uint16_t mtuSize = peer != nullptr ? peer->getMtuSize() : 0;
    mLive.insert(id);
    mConnectionCount.store(mLive.size());

    OutboundCommand adopt;
    adopt.mKind = OutboundCommand::Kind::Adopt;
    adopt.mId = id;
    adopt.mMtuSize = mtuSize;
    adopt.mConnection = std::unique_ptr<Connection>(new Connection(id, std::move(peer)));
    _pushOutbound(std::move(adopt));
}

void NetworkHandler::onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                        const std::string &message) {
    if (mLive.erase(id) == 0)
        return;

    mConnectionCount.store(mLive.size());

    // The shard reports the close once it dropped the connection, so no data of it follows the event.
    OutboundCommand remove;
    remove.mKind = OutboundCommand::Kind::Remove;
    remove.mId = id;
    remove.mReason = reason;
    remove.mData = message;
    _pushOutbound(std::move(remove));
}

#include "Network/Handler/NetworkHandler.h"

#include "Core/Utility/BinaryStream.h"
#include "Protocol/Packet.h"

#include <algorithm>
#include <chrono>
#include <utility>

NetworkHandler::Connection::Connection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer)
        : mId(id), mPeer(std::move(peer)) {
    mEncryptedPeer = std::make_shared<EncryptedNetworkPeer>(mPeer);
    mCompressedPeer = std::make_shared<CompressedNetworkPeer>(mEncryptedPeer);
    mBatchedPeer = std::make_shared<BatchedNetworkPeer>(mCompressedPeer);
}

NetworkHandler::NetworkHandler(std::unique_ptr<Connector> connector)
        : mIoRunning(false), mConnectionCount(0) {
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

    mConnections.clear();
    mConnectionCount.store(0);
}

void NetworkHandler::startIoThread() {
    if (mIoRunning.load())
        return;

    mIoRunning.store(true);
    mIoThread = std::thread(&NetworkHandler::_ioLoop, this);
}

void NetworkHandler::stopIoThread() {
    if (!mIoRunning.load())
        return;

    mIoRunning.store(false);

    if (mIoThread.joinable())
        mIoThread.join();

    mOutbound.close();
    mInbound.close();
}

void NetworkHandler::_ioLoop() {
    while (mIoRunning.load()) {
        _pumpIo();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    _pumpIo();
}

void NetworkHandler::addListener(Listener *listener) {
    if (listener != nullptr && std::find(mListeners.begin(), mListeners.end(), listener) == mListeners.end())
        mListeners.push_back(listener);
}

void NetworkHandler::removeListener(Listener *listener) {
    mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), listener), mListeners.end());
}

NetworkHandler::Connection *NetworkHandler::getConnection(const NetworkIdentifier &id) {
    auto it = mConnections.find(id);
    return it == mConnections.end() ? nullptr : it->second.get();
}

void NetworkHandler::flush(const NetworkIdentifier &id) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Flush;
    command.mId = id;
    mOutbound.push(std::move(command));
}

void NetworkHandler::enableCompression(const NetworkIdentifier &id,
                                       CompressedNetworkPeer::CompressionAlgorithm algorithm,
                                       unsigned short threshold) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::EnableCompression;
    command.mId = id;
    command.mAlgorithm = algorithm;
    command.mThreshold = threshold;
    mOutbound.push(std::move(command));
}

void NetworkHandler::enableEncryption(const NetworkIdentifier &id, const EncryptionKey &key) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::EnableEncryption;
    command.mId = id;
    command.mEncryptionKey = key;
    mOutbound.push(std::move(command));
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
    BinaryStream stream;
    packet.writeWithHeader(stream, context);

    if (!mOutboundFilter) {
        send(id, stream.getBuffer(), _toPeerReliability(packet), _toPeerCompressibility(packet));
        return;
    }

    std::string data = stream.getBuffer();
    if (!mOutboundFilter(id, data))
        return;

    send(id, data, _toPeerReliability(packet), _toPeerCompressibility(packet));
}

void NetworkHandler::setOutboundFilter(OutboundFilter filter) {
    mOutboundFilter = std::move(filter);
}

void NetworkHandler::sendToAll(const Packet &packet, const PacketCodecContext &context) {
    BinaryStream stream;
    packet.writeWithHeader(stream, context);

    sendToAll(stream.getBuffer(), _toPeerReliability(packet), _toPeerCompressibility(packet));
}

void NetworkHandler::send(const NetworkIdentifier &id, const std::string &data, NetworkPeer::Reliability reliability,
                          Compressibility compressibility) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Data;
    command.mId = id;
    command.mData = data;
    command.mReliability = reliability;
    command.mCompressibility = compressibility;
    mOutbound.push(std::move(command));
}

void NetworkHandler::sendToAll(const std::string &data, NetworkPeer::Reliability reliability,
                               Compressibility compressibility) {
    OutboundCommand command;
    command.mKind = OutboundCommand::Kind::Data;
    command.mData = data;
    command.mBroadcast = true;
    command.mReliability = reliability;
    command.mCompressibility = compressibility;
    mOutbound.push(std::move(command));
}

void NetworkHandler::_applyOutbound(OutboundCommand &command) {
    if (command.mKind == OutboundCommand::Kind::Data) {
        if (command.mBroadcast) {
            for (auto &entry: mConnections)
                entry.second->getBatchedPeer()->sendPacket(command.mData, command.mReliability,
                                                           command.mCompressibility);
            return;
        }

        Connection *connection = getConnection(command.mId);
        if (connection != nullptr)
            connection->getBatchedPeer()->sendPacket(command.mData, command.mReliability, command.mCompressibility);
        return;
    }

    Connection *connection = getConnection(command.mId);
    if (connection == nullptr)
        return;

    if (command.mKind == OutboundCommand::Kind::Flush)
        connection->getBatchedPeer()->flush();
    else if (command.mKind == OutboundCommand::Kind::EnableEncryption)
        connection->getEncryptedPeer()->enableEncryption(command.mEncryptionKey);
    else
        connection->getCompressedPeer()->enableCompression(command.mAlgorithm, command.mThreshold);
}

void NetworkHandler::_pumpIo() {
    for (std::unique_ptr<Connector> &connector: mConnectors)
        connector->runEvents();

    std::vector<OutboundCommand> commands = mOutbound.drain();
    for (OutboundCommand &command: commands)
        _applyOutbound(command);

    std::string data;

    for (auto &entry: mConnections) {
        NetworkPeer *peer = entry.second->getBatchedPeer();

        while (peer->receivePacket(data) == NetworkPeer::DataStatus::HasData) {
            InboundEvent event;
            event.mKind = InboundEvent::Kind::Data;
            event.mId = entry.first;
            event.mData = data;
            mInbound.push(std::move(event));
        }

        if (entry.second->getEncryptedPeer()->hasFailed() && !entry.second->isFailureReported()) {
            entry.second->markFailureReported();

            InboundEvent event;
            event.mKind = InboundEvent::Kind::Failed;
            event.mId = entry.first;
            mInbound.push(std::move(event));
        }

        peer->update();
        peer->flush();
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
                for (Listener *listener: mListeners)
                    listener->onNewIncomingConnection(event.mId);
                break;
            }

            case InboundEvent::Kind::Closed: {
                ProfilerScopedSection section(*mProfiler, ProfilerSection::NetworkConnection, profiling);
                for (Listener *listener: mListeners)
                    listener->onConnectionClosed(event.mId, event.mReason, event.mData);
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
    mConnections[id] = std::unique_ptr<Connection>(new Connection(id, std::move(peer)));
    mConnectionCount.store(mConnections.size());

    InboundEvent event;
    event.mKind = InboundEvent::Kind::Opened;
    event.mId = id;
    mInbound.push(std::move(event));
}

void NetworkHandler::onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                        const std::string &message) {
    if (mConnections.erase(id) == 0)
        return;

    mConnectionCount.store(mConnections.size());

    InboundEvent event;
    event.mKind = InboundEvent::Kind::Closed;
    event.mId = id;
    event.mReason = reason;
    event.mData = message;
    mInbound.push(std::move(event));
}

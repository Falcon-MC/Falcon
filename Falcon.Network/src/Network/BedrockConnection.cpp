#include "Network/BedrockConnection.h"

#include "Core/Utility/EncodingSettings.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Client/RakNetClient.h"
#include "Network/Crypto/KeyPair.h"
#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/DisconnectPacket.h"
#include "Protocol/Packets/ItemRegistryPacket.h"
#include "Protocol/Packets/JigsawStructureDataPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Packets/ServerToClientHandshakePacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/VoxelShapesPacket.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace {

    const int READ_IDLE_WAIT_MS = 1;
    const size_t SERVER_MAX_LIST_SIZE = 1024 * 1024;
    const size_t SERVER_MAX_BYTE_ARRAY_SIZE = 16 * 1024 * 1024;
    const size_t SERVER_MAX_STRING_LENGTH = 1024 * 1024;

    const EncodingSettings &serverPacketLimits() {
        static const EncodingSettings settings = []() {
            EncodingSettings limits;
            limits.mMaxListSize = SERVER_MAX_LIST_SIZE;
            limits.mMaxByteArraySize = SERVER_MAX_BYTE_ARRAY_SIZE;
            limits.mMaxStringLength = SERVER_MAX_STRING_LENGTH;
            return limits;
        }();
        return settings;
    }

}

BedrockConnection::BedrockConnection(Side side, std::shared_ptr<NetworkPeer> transport,
                                     std::shared_ptr<RakNetClient> clientTransport)
        : mSide(side), mTransport(std::move(transport)), mClientTransport(std::move(clientTransport)),
          mClientDriver(mClientTransport),
          mDefaultCodecContext(mDefaultBlockDefinitions, mDefaultItemDefinitions),
          mCodecContext(&mDefaultCodecContext), mChunkRadius(0), mClosed(false) {
    mEncryptedPeer = std::make_shared<EncryptedNetworkPeer>(mTransport, side == Side::Client);
    mCompressedPeer = std::make_shared<CompressedNetworkPeer>(mEncryptedPeer);
    mBatchedPeer = std::make_shared<BatchedNetworkPeer>(mCompressedPeer);
}

BedrockConnection::BedrockConnection(Side side, std::shared_ptr<NetworkPeer> transport,
                                     std::shared_ptr<ClientTransport> clientDriver)
        : mSide(side), mTransport(std::move(transport)), mClientDriver(std::move(clientDriver)),
          mDefaultCodecContext(mDefaultBlockDefinitions, mDefaultItemDefinitions),
          mCodecContext(&mDefaultCodecContext), mChunkRadius(0), mClosed(false) {
    mEncryptedPeer = std::make_shared<EncryptedNetworkPeer>(mTransport, side == Side::Client);
    mCompressedPeer = std::make_shared<CompressedNetworkPeer>(mEncryptedPeer);
    mBatchedPeer = std::make_shared<BatchedNetworkPeer>(mCompressedPeer);
}

BedrockConnection::~BedrockConnection() {
    close("connection destroyed");
}

void BedrockConnection::setCodecContext(const PacketCodecContext *context) {
    mCodecContext = context != nullptr ? context : &mDefaultCodecContext;
}

void BedrockConnection::enableCompression(CompressedNetworkPeer::CompressionAlgorithm algorithm,
                                          unsigned short threshold) {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    mBatchedPeer->flush();
    mCompressedPeer->enableCompression(algorithm, threshold);
}

bool BedrockConnection::enableEncryption(const EncryptionKey &key) {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    mBatchedPeer->flush();
    return mEncryptedPeer->enableEncryption(key);
}

bool BedrockConnection::startServerEncryption(const KeyPair &serverKey, const std::string &clientPublicKeyBase64) {
    std::string jwt;
    EncryptionKey key{};

    if (!EncryptionHandshake::createServerToken(serverKey, clientPublicKeyBase64, jwt, key))
        return false;

    ServerToClientHandshakePacket handshake;
    handshake.mJwt = std::move(jwt);
    send(handshake);

    return enableEncryption(key);
}

bool BedrockConnection::isCompressionEnabled() const {
    return mCompressedPeer->isCompressionEnabled();
}

bool BedrockConnection::isEncryptionEnabled() const {
    return mEncryptedPeer->isEncryptionEnabled();
}

NetworkPeer::Reliability BedrockConnection::toPeerReliability(const Packet &packet) {
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

Compressibility BedrockConnection::toPeerCompressibility(const Packet &packet) {
    return packet.mCompressible == Packet::Compressibility::Incompressible
           ? Compressibility::Incompressible
           : Compressibility::Compressible;
}

bool BedrockConnection::peekPacketId(const std::string &payload, MinecraftPacketIds &outId) {
    uint32_t header = 0;
    int shift = 0;

    for (size_t index = 0; index < payload.size() && shift < 35; ++index, shift += 7) {
        const unsigned char byte = (unsigned char) payload[index];
        header |= (uint32_t) (byte & 0x7f) << shift;

        if ((byte & 0x80) == 0) {
            outId = (MinecraftPacketIds) (header & 0x3ff);
            return true;
        }
    }

    return false;
}

void BedrockConnection::send(const Packet &packet) {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    if (mClosed.load())
        return;

    mSendStream.reset();
    packet.writeWithHeader(mSendStream, *mCodecContext);
    mBatchedPeer->sendPacket(mSendStream.getBuffer(), toPeerReliability(packet), toPeerCompressibility(packet));
}

void BedrockConnection::sendRaw(const std::string &payload, NetworkPeer::Reliability reliability,
                                Compressibility compressibility) {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    if (mClosed.load())
        return;

    mBatchedPeer->sendPacket(payload, reliability, compressibility);
}

void BedrockConnection::deferRaw(std::string payload) {
    mDeferred.push_back(std::move(payload));
}

void BedrockConnection::_handleDisconnectPacket(std::string payload) {
    std::string message = "disconnected by the remote system";

    std::shared_ptr<Packet> packet = decode(std::move(payload));
    const DisconnectPacket *disconnect = dynamic_cast<const DisconnectPacket *>(packet.get());

    if (disconnect != nullptr && !disconnect->mMessageSkipped && !disconnect->mKickMessage.empty())
        message = disconnect->mKickMessage;

    close(message);
}

bool BedrockConnection::receiveRaw(std::string &outPayload) {
    if (!mDeferred.empty()) {
        outPayload = std::move(mDeferred.front());
        mDeferred.pop_front();
        return true;
    }

    if (mClosed.load())
        return false;

    for (;;) {
        if (mBatchedPeer->receivePacket(outPayload) != NetworkPeer::DataStatus::HasData) {
            if (mBatchedPeer->hasFailed())
                close("malformed packet batch received");
            else if (mEncryptedPeer->hasFailed())
                close("invalid encrypted packet received");
            else if (mClientDriver != nullptr && !mClientDriver->isConnected())
                close(toString(mClientDriver->getCloseReason()));

            return false;
        }

        MinecraftPacketIds id;
        if (!peekPacketId(outPayload, id)) {
            close("malformed packet header received");
            return false;
        }

        if (id == MinecraftPacketIds::Disconnect) {
            _handleDisconnectPacket(std::move(outPayload));
            return false;
        }

        return true;
    }
}

std::shared_ptr<Packet> BedrockConnection::decode(std::string payload) const {
    MinecraftPacketIds id;
    if (!peekPacketId(payload, id))
        return nullptr;

    const ProtocolCodec &codec = mCodecContext->getCodec();
    std::shared_ptr<Packet> packet = codec.createPacket(id);
    if (packet == nullptr)
        return nullptr;

    ReadOnlyBinaryStream stream(std::move(payload));
    if (mSide == Side::Client)
        stream.setEncodingSettings(serverPacketLimits());

    try {
        packet->readHeader(stream);
        codec.read(*packet, stream, *mCodecContext);
    } catch (const std::exception &exception) {
        std::lock_guard<std::mutex> guard(mReasonMutex);
        mLastDecodeError = exception.what();
        return nullptr;
    } catch (...) {
        std::lock_guard<std::mutex> guard(mReasonMutex);
        mLastDecodeError = "unknown error";
        return nullptr;
    }

    return packet;
}

std::string BedrockConnection::getLastDecodeError() const {
    std::lock_guard<std::mutex> guard(mReasonMutex);
    return mLastDecodeError;
}

std::shared_ptr<Packet> BedrockConnection::receive() {
    std::string payload;

    while (receiveRaw(payload)) {
        std::shared_ptr<Packet> packet = decode(std::move(payload));
        if (packet != nullptr)
            return packet;
    }

    return nullptr;
}

void BedrockConnection::update() {
    if (mClientDriver != nullptr)
        mClientDriver->runEvents();

    if (!mClosed.load())
        mBatchedPeer->update();
}

void BedrockConnection::flush() {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    if (mClosed.load())
        return;

    mBatchedPeer->flush();
}

void BedrockConnection::disconnect(const std::string &message, int reason) {
    std::lock_guard<std::recursive_mutex> guard(mSendMutex);
    if (mClosed.load())
        return;

    DisconnectPacket packet;
    packet.mReason = reason;
    packet.mMessageSkipped = message.empty();
    packet.mKickMessage = message;
    packet.mFilteredMessage = message;

    send(packet);
    flush();
    close(message.empty() ? "disconnected" : message);
}

void BedrockConnection::close(const std::string &reason) {
    if (mClosed.exchange(true))
        return;

    {
        std::lock_guard<std::mutex> guard(mReasonMutex);
        mDisconnectReason = reason;
    }

    {
        std::lock_guard<std::recursive_mutex> guard(mSendMutex);
        mBatchedPeer->flush();
    }

    if (mClientDriver != nullptr)
        mClientDriver->close();
}

std::string BedrockConnection::getDisconnectReason() const {
    std::lock_guard<std::mutex> guard(mReasonMutex);
    return mDisconnectReason;
}

NetworkPeer::NetworkStatus BedrockConnection::getNetworkStatus() const {
    return mBatchedPeer->getNetworkStatus();
}

void BedrockConnection::setGameData(std::shared_ptr<StartGamePacket> startGame,
                                    std::shared_ptr<ItemRegistryPacket> itemRegistry, int chunkRadius) {
    mStartGame = std::move(startGame);
    mItemRegistry = std::move(itemRegistry);
    mChunkRadius = chunkRadius;
}

bool BedrockConnection::readRaw(std::string &outPayload, int timeoutMs, const std::atomic<bool> *cancel) {
    const auto start = std::chrono::steady_clock::now();

    for (;;) {
        update();
        if (receiveRaw(outPayload))
            return true;

        if (mClosed.load() || (cancel != nullptr && cancel->load()))
            return false;

        flush();

        if (timeoutMs >= 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs)
                return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(READ_IDLE_WAIT_MS));
    }
}

std::shared_ptr<Packet> BedrockConnection::readPacket(int timeoutMs, const std::atomic<bool> *cancel) {
    std::string payload;

    while (readRaw(payload, timeoutMs, cancel)) {
        std::shared_ptr<Packet> packet = decode(std::move(payload));
        if (packet != nullptr)
            return packet;
    }

    return nullptr;
}

bool BedrockConnection::_waitFor(MinecraftPacketIds id, int timeoutMs, const std::atomic<bool> *cancel,
                                 std::deque<std::string> &skipped, std::string &outPayload,
                                 std::string &outError) {
    const auto start = std::chrono::steady_clock::now();

    for (;;) {
        int remaining = -1;
        if (timeoutMs >= 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
            remaining = (int) std::max<long long>(0, (long long) timeoutMs - elapsed);
        }

        if (!readRaw(outPayload, remaining, cancel)) {
            outError = mClosed.load() ? "connection closed: " + getDisconnectReason() : "timed out";
            return false;
        }

        MinecraftPacketIds received;
        if (peekPacketId(outPayload, received) && received == id)
            return true;

        skipped.push_back(std::move(outPayload));
    }
}

void BedrockConnection::_restoreSkipped(std::deque<std::string> &skipped) {
    while (!skipped.empty()) {
        mDeferred.push_front(std::move(skipped.back()));
        skipped.pop_back();
    }
}

bool BedrockConnection::startGame(const StartGamePacket &startGame, const ItemRegistryPacket *itemRegistry,
                                  int maxChunkRadius, unsigned int timeoutMs, const std::atomic<bool> *cancel,
                                  std::string &outError) {
    JigsawStructureDataPacket structures;
    structures.mJigsawStructureData.put("processors", Tag::ofList(Tag::Type::Compound));
    structures.mJigsawStructureData.put("template_pools", Tag::ofList(Tag::Type::Compound));
    structures.mJigsawStructureData.put("jigsaws", Tag::ofList(Tag::Type::Compound));
    structures.mJigsawStructureData.put("structure_sets", Tag::ofList(Tag::Type::Compound));
    send(structures);

    VoxelShapesPacket shapes;
    send(shapes);

    send(startGame);
    if (itemRegistry != nullptr)
        send(*itemRegistry);
    flush();

    std::deque<std::string> skipped;
    std::string payload;

    if (!_waitFor(MinecraftPacketIds::RequestChunkRadius, (int) timeoutMs, cancel, skipped, payload, outError)) {
        _restoreSkipped(skipped);
        return false;
    }

    std::shared_ptr<RequestChunkRadiusPacket> request =
            std::dynamic_pointer_cast<RequestChunkRadiusPacket>(decode(std::move(payload)));
    const int requested = request != nullptr ? request->mRadius : maxChunkRadius;

    ChunkRadiusUpdatedPacket radius;
    radius.mRadius = maxChunkRadius > 0 ? std::min(requested, maxChunkRadius) : requested;
    send(radius);

    PlayStatusPacket status;
    status.mStatus = PlayStatusPacket::Status::PlayerSpawn;
    send(status);
    flush();

    const bool initialized = _waitFor(MinecraftPacketIds::SetLocalPlayerAsInitialized, (int) timeoutMs, cancel,
                                      skipped, payload, outError);
    _restoreSkipped(skipped);
    return initialized;
}

bool BedrockConnection::spawn(unsigned int timeoutMs, const std::atomic<bool> *cancel, std::string &outError) {
    std::deque<std::string> skipped;

    while (!mSpawnReceived) {
        std::string payload;
        if (!_waitFor(MinecraftPacketIds::PlayStatus, (int) timeoutMs, cancel, skipped, payload, outError)) {
            _restoreSkipped(skipped);
            return false;
        }

        std::shared_ptr<PlayStatusPacket> status =
                std::dynamic_pointer_cast<PlayStatusPacket>(decode(payload));
        if (status != nullptr && status->mStatus == PlayStatusPacket::Status::PlayerSpawn)
            mSpawnReceived = true;
        else
            skipped.push_back(std::move(payload));
    }

    _restoreSkipped(skipped);

    SetLocalPlayerAsInitializedPacket initialized;
    initialized.mRuntimeActorId = mStartGame != nullptr ? mStartGame->mRuntimeActorId : 0;
    send(initialized);
    flush();
    return true;
}

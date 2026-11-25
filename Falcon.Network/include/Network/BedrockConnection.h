#pragma once

#include "Network/BatchedNetworkPeer.h"
#include "Network/CompressedNetworkPeer.h"
#include "Network/Crypto/EncryptionHandshake.h"
#include "Network/EncryptedNetworkPeer.h"
#include "Network/NetworkEnums.h"
#include "Network/NetworkPeer.h"
#include "Protocol/MinecraftPacketIds.h"
#include "Protocol/Packet.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class KeyPair;

class RakNetClient;

class ClientTransport;

class StartGamePacket;

class ItemRegistryPacket;

class BedrockConnection {
public:
    enum class Side : int {
        Client = 0,
        Server = 1
    };

    BedrockConnection(Side side, std::shared_ptr<NetworkPeer> transport,
                      std::shared_ptr<RakNetClient> clientTransport = nullptr);

    BedrockConnection(Side side, std::shared_ptr<NetworkPeer> transport,
                      std::shared_ptr<ClientTransport> clientDriver);

    ~BedrockConnection();

    BedrockConnection(const BedrockConnection &) = delete;

    BedrockConnection &operator=(const BedrockConnection &) = delete;

    Side getSide() const {
        return mSide;
    }

    void setCodecContext(const PacketCodecContext *context);

    const PacketCodecContext &getCodecContext() const {
        return *mCodecContext;
    }

    void enableCompression(CompressedNetworkPeer::CompressionAlgorithm algorithm, unsigned short threshold);

    bool enableEncryption(const EncryptionKey &key);

    bool startServerEncryption(const KeyPair &serverKey, const std::string &clientPublicKeyBase64);

    bool isCompressionEnabled() const;

    bool isEncryptionEnabled() const;

    void send(const Packet &packet);

    void sendRaw(const std::string &payload,
                 NetworkPeer::Reliability reliability = NetworkPeer::Reliability::ReliableOrdered,
                 Compressibility compressibility = Compressibility::Compressible);

    bool receiveRaw(std::string &outPayload);

    std::shared_ptr<Packet> receive();

    std::shared_ptr<Packet> decode(std::string payload) const;

    std::string getLastDecodeError() const;

    void deferRaw(std::string payload);

    void update();

    void flush();

    void disconnect(const std::string &message, int reason = 0);

    void close(const std::string &reason);

    bool isClosed() const {
        return mClosed.load();
    }

    std::string getDisconnectReason() const;

    NetworkPeer::NetworkStatus getNetworkStatus() const;

    const std::shared_ptr<NetworkPeer> &getTransport() const {
        return mTransport;
    }

    const std::shared_ptr<RakNetClient> &getClientTransport() const {
        return mClientTransport;
    }

    const std::shared_ptr<ClientTransport> &getClientDriver() const {
        return mClientDriver;
    }

    const std::shared_ptr<StartGamePacket> &getStartGame() const {
        return mStartGame;
    }

    const std::shared_ptr<ItemRegistryPacket> &getItemRegistry() const {
        return mItemRegistry;
    }

    int getChunkRadius() const {
        return mChunkRadius;
    }

    void setGameData(std::shared_ptr<StartGamePacket> startGame, std::shared_ptr<ItemRegistryPacket> itemRegistry,
                     int chunkRadius);

    bool readRaw(std::string &outPayload, int timeoutMs = -1, const std::atomic<bool> *cancel = nullptr);

    std::shared_ptr<Packet> readPacket(int timeoutMs = -1, const std::atomic<bool> *cancel = nullptr);

    bool startGame(const StartGamePacket &startGame, const ItemRegistryPacket *itemRegistry, int maxChunkRadius,
                   unsigned int timeoutMs, const std::atomic<bool> *cancel, std::string &outError);

    bool spawn(unsigned int timeoutMs, const std::atomic<bool> *cancel, std::string &outError);

    void setSpawnReceived(bool received) {
        mSpawnReceived = received;
    }

    bool isSpawnReceived() const {
        return mSpawnReceived;
    }

    static bool peekPacketId(const std::string &payload, MinecraftPacketIds &outId);

    static NetworkPeer::Reliability toPeerReliability(const Packet &packet);

    static Compressibility toPeerCompressibility(const Packet &packet);

private:
    void _handleDisconnectPacket(std::string payload);

    bool _waitFor(MinecraftPacketIds id, int timeoutMs, const std::atomic<bool> *cancel,
                  std::deque<std::string> &skipped, std::string &outPayload, std::string &outError);

    void _restoreSkipped(std::deque<std::string> &skipped);

    Side mSide;
    std::shared_ptr<NetworkPeer> mTransport;
    std::shared_ptr<RakNetClient> mClientTransport;
    std::shared_ptr<ClientTransport> mClientDriver;
    std::shared_ptr<EncryptedNetworkPeer> mEncryptedPeer;
    std::shared_ptr<CompressedNetworkPeer> mCompressedPeer;
    std::shared_ptr<BatchedNetworkPeer> mBatchedPeer;

    BlockDefinitionRegistry mDefaultBlockDefinitions;
    ItemDefinitionRegistry mDefaultItemDefinitions;
    PacketCodecContext mDefaultCodecContext;
    const PacketCodecContext *mCodecContext;

    BinaryStream mSendStream;
    std::deque<std::string> mDeferred;

    std::shared_ptr<StartGamePacket> mStartGame;
    std::shared_ptr<ItemRegistryPacket> mItemRegistry;
    int mChunkRadius;
    bool mSpawnReceived = false;

    std::recursive_mutex mSendMutex;
    std::atomic<bool> mClosed;
    mutable std::mutex mReasonMutex;
    std::string mDisconnectReason;
    mutable std::string mLastDecodeError;
};

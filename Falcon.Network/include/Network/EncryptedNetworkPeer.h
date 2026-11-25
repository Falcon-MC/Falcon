#pragma once

#include "Network/Crypto/EncryptionHandshake.h"
#include "Network/NetworkPeer.h"

#include <cstdint>
#include <memory>
#include <string>

struct evp_cipher_ctx_st;
struct evp_md_ctx_st;

class EncryptedNetworkPeer : public NetworkPeer {
public:
    // plainTextGrace: a client keeps reading clear packets until the first encrypted one verifies, as some
    // servers flush one after the handshake. never on a server: a login could then go on without its key
    explicit EncryptedNetworkPeer(std::shared_ptr<NetworkPeer> peer, bool plainTextGrace = false);

    ~EncryptedNetworkPeer() override;

    EncryptedNetworkPeer(const EncryptedNetworkPeer &) = delete;

    EncryptedNetworkPeer &operator=(const EncryptedNetworkPeer &) = delete;

    bool enableEncryption(const EncryptionKey &key);

    bool isEncryptionEnabled() const {
        return mEncryptCipher != nullptr;
    }

    bool hasFailed() const {
        return mFailed;
    }

    void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override;

    DataStatus receivePacket(std::string &outData) override;

    NetworkStatus getNetworkStatus() const override;

    void update() override;

    void flush() override;

    bool usesGamePacketId() const override;

private:
    bool _checksum(evp_md_ctx_st *digest, uint64_t counter, const char *data, size_t length, uint8_t out[8]);

    DataStatus _receiveFirst(std::string &outData, size_t offset);

    std::shared_ptr<NetworkPeer> mPeer;
    EncryptionKey mKey{};
    evp_cipher_ctx_st *mEncryptCipher = nullptr;
    evp_cipher_ctx_st *mDecryptCipher = nullptr;
    evp_md_ctx_st *mSendDigest = nullptr;
    evp_md_ctx_st *mReceiveDigest = nullptr;
    std::string mSendBuffer;
    uint64_t mSendCounter = 0;
    uint64_t mReceiveCounter = 0;
    bool mFailed = false;
    bool mPlainTextGrace = false;
};

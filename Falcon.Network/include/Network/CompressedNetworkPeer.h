#pragma once

#include "Network/NetworkPeer.h"

#include <memory>

class CompressedNetworkPeer : public NetworkPeer {
public:
    enum class CompressionAlgorithm : unsigned char {
        ZLib = 0,
        Snappy = 1,
        None = 0xff
    };

    static const unsigned char GAME_PACKET_ID = 0xfe;

    // how the other side compressed the batches it sent, counted over its first batches
    struct BatchTraits {
        unsigned short mBatches = 0;
        // compressed although shorter than the threshold it was given
        unsigned short mCompressedBelowThreshold = 0;
        // an algorithm other than the one it was told to use
        unsigned short mOtherAlgorithm = 0;
        // deflate batches compared with what the game's compressor writes for the same bytes
        unsigned short mCompared = 0;
        unsigned short mNotReproduced = 0;
    };

    explicit CompressedNetworkPeer(std::shared_ptr<NetworkPeer> peer);

    void enableCompression(CompressionAlgorithm algorithm, unsigned short threshold);

    void disableCompression();

    bool isCompressionEnabled() const { return mCompressionEnabled; }

    const BatchTraits &getBatchTraits() const { return mBatchTraits; }

    void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override;

    DataStatus receivePacket(std::string &outData) override;

    NetworkStatus getNetworkStatus() const override;

    void update() override;

    void flush() override;

    void sendSharedBatch(const SharedPacket &packet, Reliability reliability) override;

    static bool compress(CompressionAlgorithm algorithm, const std::string &input, std::string &outData);

private:
    static bool _compressZlib(const std::string &input, std::string &outData);

    static bool _decompressZlib(const char *data, size_t length, std::string &outData);

    static bool _compressSnappy(const std::string &input, std::string &outData);

    void _observe(CompressionAlgorithm algorithm, const char *body, size_t length, const std::string &batch);

    static bool _decompressSnappy(const char *data, size_t length, std::string &outData);

    std::shared_ptr<NetworkPeer> mPeer;
    CompressionAlgorithm mAlgorithm;
    unsigned short mCompressionThreshold;
    bool mCompressionEnabled;
    BatchTraits mBatchTraits;
};

#pragma once

#include "Network/CompressedNetworkPeer.h"

#include <mutex>
#include <string>

/**
 * One encoded game packet sent to many peers: framed as a batch of its own and compressed once per algorithm,
 * on first use, by whichever thread sends it first.
 */
class SharedPacket {
public:
    explicit SharedPacket(const std::string &packet);

    // varint length then the packet: the batch payload before compression
    const std::string &getBatch() const {
        return mBatch;
    }

    size_t getPacketSize() const {
        return mPacketSize;
    }

    std::string getPacket() const {
        return mBatch.substr(mBatch.size() - mPacketSize);
    }

    // empty when the algorithm leaves the batch uncompressed or compression failed; thread safe
    const std::string &getCompressed(CompressedNetworkPeer::CompressionAlgorithm algorithm) const;

private:
    struct Compressed {
        bool mReady = false;
        std::string mData;
    };

    std::string mBatch;
    size_t mPacketSize;
    mutable std::mutex mMutex;
    mutable Compressed mZlib;
    mutable Compressed mSnappy;
};

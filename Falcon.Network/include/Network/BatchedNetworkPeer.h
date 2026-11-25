#pragma once

#include "Network/NetworkPeer.h"

#include <deque>
#include <memory>
#include <vector>
#include <utility>

class BatchedNetworkPeer : public NetworkPeer {
public:
    explicit BatchedNetworkPeer(std::shared_ptr<NetworkPeer> peer);

    void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override;

    DataStatus receivePacket(std::string &outData) override;

    NetworkStatus getNetworkStatus() const override;

    void update() override;

    void flush() override;

    // after the pending batch: packet order holds across both paths
    void sendShared(const SharedPacket &packet, Reliability reliability);

    bool hasFailed() const { return mFailed; }

private:
    bool _unbatch(std::string batch);

    /**
     * Hands the pending batch to the transport with its own reliability, so packets of different reliabilities
     * never share a batch.
     */
    void _sendBatch();

    std::shared_ptr<NetworkPeer> mPeer;

    static const size_t MAX_BATCH_SIZE = 1024 * 1024;

    std::string mBatchBuffer;
    Reliability mBatchReliability;
    Compressibility mBatchCompressibility;

    std::string mIncomingBatch;
    std::vector<std::pair<size_t, size_t>> mIncomingPackets;
    size_t mIncomingIndex = 0;
    bool mFailed = false;
};

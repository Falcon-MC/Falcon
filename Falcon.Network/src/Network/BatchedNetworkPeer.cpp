#include "Network/BatchedNetworkPeer.h"

#include "Network/SharedPacket.h"

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

BatchedNetworkPeer::BatchedNetworkPeer(std::shared_ptr<NetworkPeer> peer)
        : mPeer(std::move(peer)), mBatchReliability(Reliability::ReliableOrdered),
          mBatchCompressibility(Compressibility::Compressible) {}

void BatchedNetworkPeer::sendPacket(const std::string &data, Reliability reliability,
                                    Compressibility compressibility) {
    if (data.empty())
        return;

    if (!mBatchBuffer.empty() && reliability != mBatchReliability)
        _sendBatch();

    if (compressibility == Compressibility::Incompressible)
        mBatchCompressibility = Compressibility::Incompressible;

    mBatchReliability = reliability;

    BinaryStream stream;
    stream.putUnsignedVarInt((uint32_t) data.size());

    mBatchBuffer.append(stream.getBuffer());
    mBatchBuffer.append(data);

    if (mBatchBuffer.size() >= MAX_BATCH_SIZE)
        flush();
}

void BatchedNetworkPeer::sendShared(const SharedPacket &packet, Reliability reliability) {
    if (packet.getPacketSize() == 0)
        return;

    _sendBatch();
    mPeer->sendSharedBatch(packet, reliability);
}

void BatchedNetworkPeer::flush() {
    _sendBatch();
    mPeer->flush();
}

void BatchedNetworkPeer::_sendBatch() {
    if (mBatchBuffer.empty())
        return;

    mPeer->sendPacket(mBatchBuffer, mBatchReliability, mBatchCompressibility);

    mBatchBuffer.clear();
    mBatchCompressibility = Compressibility::Compressible;
}

bool BatchedNetworkPeer::_unbatch(std::string batch) {
    std::vector<std::pair<size_t, size_t>> frames;
    size_t cursor = 0;
    while (cursor < batch.size()) {
        if (frames.size() >= 65536) return false;
        uint32_t length = 0;
        bool complete = false;
        for (unsigned shift = 0; shift < 35; shift += 7) {
            if (cursor == batch.size()) return false;
            const auto byte = static_cast<unsigned char>(batch[cursor++]);
            if (shift == 28 && (byte & 0xf0)) return false;
            length |= uint32_t(byte & 0x7f) << shift;
            if (!(byte & 0x80)) { complete = true; break; }
        }
        if (!complete || length == 0 || length > batch.size() - cursor) return false;
        frames.emplace_back(cursor, length);
        cursor += length;
    }
    if (frames.empty()) return false;
    mIncomingBatch = std::move(batch);
    mIncomingPackets = std::move(frames);
    mIncomingIndex = 0;
    return true;
}

NetworkPeer::DataStatus BatchedNetworkPeer::receivePacket(std::string &outData) {
    if (mFailed) return DataStatus::NoData;
    if (mIncomingIndex == mIncomingPackets.size()) {
        mIncomingBatch.clear();
        mIncomingPackets.clear();
        mIncomingIndex = 0;
        std::string batch;
        if (mPeer->receivePacket(batch) == DataStatus::NoData) return DataStatus::NoData;
        if (!_unbatch(std::move(batch))) { mFailed = true; return DataStatus::NoData; }
    }
    const auto [offset, length] = mIncomingPackets[mIncomingIndex++];
    outData.assign(mIncomingBatch, offset, length);
    return DataStatus::HasData;
}

NetworkPeer::NetworkStatus BatchedNetworkPeer::getNetworkStatus() const {
    return mPeer->getNetworkStatus();
}

void BatchedNetworkPeer::update() {
    mPeer->update();
}

#include "Network/SharedPacket.h"

#include "Core/Utility/BinaryStream.h"

SharedPacket::SharedPacket(const std::string &packet) : mPacketSize(packet.size()) {
    BinaryStream stream;
    stream.putUnsignedVarInt((uint32_t) packet.size());
    mBatch = stream.getBuffer();
    mBatch.append(packet);
}

const std::string &SharedPacket::getCompressed(CompressedNetworkPeer::CompressionAlgorithm algorithm) const {
    static const std::string none;
    Compressed *slot;
    if (algorithm == CompressedNetworkPeer::CompressionAlgorithm::ZLib)
        slot = &mZlib;
    else if (algorithm == CompressedNetworkPeer::CompressionAlgorithm::Snappy)
        slot = &mSnappy;
    else
        return none;

    std::lock_guard<std::mutex> lock(mMutex);
    if (!slot->mReady) {
        slot->mReady = true;
        if (!CompressedNetworkPeer::compress(algorithm, mBatch, slot->mData))
            slot->mData.clear();
    }
    return slot->mData;
}

void NetworkPeer::sendSharedBatch(const SharedPacket &packet, Reliability reliability) {
    sendPacket(packet.getBatch(), reliability, Compressibility::Compressible);
}

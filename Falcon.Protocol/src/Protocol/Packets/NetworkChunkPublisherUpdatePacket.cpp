#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

NetworkChunkPublisherUpdatePacket::NetworkChunkPublisherUpdatePacket()
        : mRadius(0) {}

void NetworkChunkPublisherUpdatePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVector3i(mPosition);
    stream.putUnsignedVarInt(mRadius);

    // the saved chunk count is not compressed
    stream.putLInt((uint32_t) mSavedChunks.size());
    for (const Vector2i &savedChunk: mSavedChunks) {
        stream.putVarInt(savedChunk.x);
        stream.putVarInt(savedChunk.y);
    }
}

void NetworkChunkPublisherUpdatePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPosition = stream.getVector3i();
    mRadius = stream.getUnsignedVarInt();

    const uint32_t count = stream.getLInt();
    if (count > stream.getEncodingSettings().mMaxListSize) {
        throw BinaryDataException("Tried to read " + std::to_string(count) +
                                  " saved chunks but the maximum is " +
                                  std::to_string(stream.getEncodingSettings().mMaxListSize));
    }

    mSavedChunks.clear();
    mSavedChunks.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        const int32_t x = stream.getVarInt();
        const int32_t y = stream.getVarInt();
        mSavedChunks.push_back(Vector2i(x, y));
    }
}

void NetworkChunkPublisherUpdatePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

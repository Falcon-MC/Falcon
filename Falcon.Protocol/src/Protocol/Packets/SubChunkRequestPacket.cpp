#include "Protocol/Packets/SubChunkRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SubChunkRequestPacket::SubChunkRequestPacket()
        : mDimension(0) {}

void SubChunkRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mDimension);

    stream.putArrayLength((uint32_t) mPositionOffsets.size());
    for (const Vector3i &offset: mPositionOffsets) {
        stream.putByte((unsigned char) (int8_t) offset.x);
        stream.putByte((unsigned char) (int8_t) offset.y);
        stream.putByte((unsigned char) (int8_t) offset.z);
    }

    stream.putLInt((uint32_t) mSubChunkPosition.x);
    stream.putLInt((uint32_t) mSubChunkPosition.y);
    stream.putLInt((uint32_t) mSubChunkPosition.z);
}

void SubChunkRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDimension = stream.getVarInt();

    const uint32_t count = stream.getUnsignedVarInt();
    if (count > MAX_SUB_CHUNKS) {
        throw BinaryDataException("Tried to request " + std::to_string(count) +
                                  " sub chunks but the maximum is " + std::to_string(MAX_SUB_CHUNKS));
    }

    mPositionOffsets.clear();
    mPositionOffsets.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        const int8_t x = (int8_t) stream.getByte();
        const int8_t y = (int8_t) stream.getByte();
        const int8_t z = (int8_t) stream.getByte();
        mPositionOffsets.push_back(Vector3i(x, y, z));
    }

    const int32_t positionX = stream.getSignedLInt();
    const int32_t positionY = stream.getSignedLInt();
    const int32_t positionZ = stream.getSignedLInt();
    mSubChunkPosition = Vector3i(positionX, positionY, positionZ);
}

void SubChunkRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

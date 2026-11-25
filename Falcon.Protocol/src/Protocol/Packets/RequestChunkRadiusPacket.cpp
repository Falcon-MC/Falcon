#include "Protocol/Packets/RequestChunkRadiusPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RequestChunkRadiusPacket::RequestChunkRadiusPacket()
        : mRadius(0), mMaxRadius(0) {}

void RequestChunkRadiusPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mRadius);
    stream.putByte((unsigned char) mMaxRadius);
}

void RequestChunkRadiusPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRadius = stream.getVarInt();
    mMaxRadius = stream.getByte();
}

void RequestChunkRadiusPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

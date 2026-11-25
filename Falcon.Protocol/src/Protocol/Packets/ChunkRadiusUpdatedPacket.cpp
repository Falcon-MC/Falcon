#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ChunkRadiusUpdatedPacket::ChunkRadiusUpdatedPacket()
        : mRadius(0) {}

void ChunkRadiusUpdatedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mRadius);
}

void ChunkRadiusUpdatedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRadius = stream.getVarInt();
}

void ChunkRadiusUpdatedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

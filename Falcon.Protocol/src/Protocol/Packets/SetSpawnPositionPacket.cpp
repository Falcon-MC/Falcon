#include "Protocol/Packets/SetSpawnPositionPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetSpawnPositionPacket::SetSpawnPositionPacket()
        : mSpawnType(Type::PlayerSpawn), mDimensionId(0), mSpawnPosition(Vector3i(-2147483647 - 1, -2147483647 - 1, -2147483647 - 1)) {}

void SetSpawnPositionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mSpawnType);
    stream.putBlockPosition(mBlockPosition);
    stream.putVarInt(mDimensionId);
    stream.putBlockPosition(mSpawnPosition);
}

void SetSpawnPositionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSpawnType = (Type) stream.getVarInt();
    mBlockPosition = stream.getBlockPosition();
    mDimensionId = stream.getVarInt();
    mSpawnPosition = stream.getBlockPosition();
}

void SetSpawnPositionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

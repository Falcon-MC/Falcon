#include "Protocol/Packets/PlayerActionPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerActionPacket::PlayerActionPacket() = default;

void PlayerActionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    stream.putVarInt((int32_t) mAction);
    stream.putBlockPosition(mBlockPosition);
    stream.putBlockPosition(mResultPosition);
    stream.putVarInt(mFace);
}

void PlayerActionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mAction = (PlayerActionType) stream.getVarInt();
    mBlockPosition = stream.getBlockPosition();
    mResultPosition = stream.getBlockPosition();
    mFace = stream.getVarInt();
}

void PlayerActionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

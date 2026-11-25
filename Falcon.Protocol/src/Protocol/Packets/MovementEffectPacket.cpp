#include "Protocol/Packets/MovementEffectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

MovementEffectPacket::MovementEffectPacket() = default;

void MovementEffectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mActorRuntimeId);
    stream.putUnsignedVarInt((uint32_t) (int32_t) mEffectType);
    stream.putUnsignedVarInt(mDuration);
    stream.putUnsignedVarLong(mTick);
}

void MovementEffectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mActorRuntimeId = stream.getUnsignedVarLong();
    mEffectType = (MovementEffectType) (int32_t) stream.getUnsignedVarInt();
    mDuration = stream.getUnsignedVarInt();
    mTick = stream.getUnsignedVarLong();
}

void MovementEffectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

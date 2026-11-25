#include "Protocol/Packets/AgentAnimationPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AgentAnimationPacket::AgentAnimationPacket() = default;

void AgentAnimationPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAnimation);
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
}

void AgentAnimationPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAnimation = stream.getSignedByte();
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
}

void AgentAnimationPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

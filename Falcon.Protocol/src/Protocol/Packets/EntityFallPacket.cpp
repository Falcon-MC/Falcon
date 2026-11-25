#include "Protocol/Packets/EntityFallPacket.h"

#include "Protocol/NetworkPacketHandler.h"

EntityFallPacket::EntityFallPacket() = default;

void EntityFallPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putLFloat(mFallDistance);
    stream.putBool(mInVoid);
}

void EntityFallPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mFallDistance = stream.getLFloat();
    mInVoid = stream.getBool();
}

void EntityFallPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

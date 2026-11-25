#include "Protocol/Packets/AddPaintingPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AddPaintingPacket::AddPaintingPacket() = default;

void AddPaintingPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putVector3f(mPosition);
    stream.putVarInt(mDirection);
    stream.putString(mMotive);
}

void AddPaintingPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mRuntimeActorId = stream.getUnsignedVarLong();
    mPosition = stream.getVector3f();
    mDirection = stream.getVarInt();
    mMotive = stream.getString();
}

void AddPaintingPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

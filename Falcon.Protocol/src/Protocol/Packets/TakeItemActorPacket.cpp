#include "Protocol/Packets/TakeItemActorPacket.h"

#include "Protocol/NetworkPacketHandler.h"

TakeItemActorPacket::TakeItemActorPacket()
        : mItemRuntimeActorId(0), mRuntimeActorId(0) {}

void TakeItemActorPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mItemRuntimeActorId);
    stream.putUnsignedVarLong(mRuntimeActorId);
}

void TakeItemActorPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mItemRuntimeActorId = stream.getUnsignedVarLong();
    mRuntimeActorId = stream.getUnsignedVarLong();
}

void TakeItemActorPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

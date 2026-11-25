#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetLocalPlayerAsInitializedPacket::SetLocalPlayerAsInitializedPacket()
        : mRuntimeActorId(0) {}

void SetLocalPlayerAsInitializedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
}

void SetLocalPlayerAsInitializedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
}

void SetLocalPlayerAsInitializedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

#include "Protocol/Packets/RemoveActorPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RemoveActorPacket::RemoveActorPacket()
        : mUniqueActorId(0) {}

void RemoveActorPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
}

void RemoveActorPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
}

void RemoveActorPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

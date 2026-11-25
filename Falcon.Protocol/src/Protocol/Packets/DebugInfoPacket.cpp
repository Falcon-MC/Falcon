#include "Protocol/Packets/DebugInfoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

DebugInfoPacket::DebugInfoPacket()
        : mUniqueActorId(0) {}

void DebugInfoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putString(mData);
}

void DebugInfoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mData = stream.getString();
}

void DebugInfoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

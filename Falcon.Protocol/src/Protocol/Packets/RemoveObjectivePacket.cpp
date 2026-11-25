#include "Protocol/Packets/RemoveObjectivePacket.h"

#include "Protocol/NetworkPacketHandler.h"

RemoveObjectivePacket::RemoveObjectivePacket() {}

void RemoveObjectivePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mObjectiveId);
}

void RemoveObjectivePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mObjectiveId = stream.getString();
}

void RemoveObjectivePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

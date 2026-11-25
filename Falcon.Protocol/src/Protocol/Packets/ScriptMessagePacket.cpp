#include "Protocol/Packets/ScriptMessagePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ScriptMessagePacket::ScriptMessagePacket() {}

void ScriptMessagePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mChannel);
    stream.putString(mMessage);
}

void ScriptMessagePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mChannel = stream.getString();
    mMessage = stream.getString();
}

void ScriptMessagePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

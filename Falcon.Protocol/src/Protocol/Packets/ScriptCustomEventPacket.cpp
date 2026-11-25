#include "Protocol/Packets/ScriptCustomEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ScriptCustomEventPacket::ScriptCustomEventPacket() = default;

void ScriptCustomEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mEventName);
    stream.putString(mData);
}

void ScriptCustomEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mEventName = stream.getString();
    mData = stream.getString();
}

void ScriptCustomEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

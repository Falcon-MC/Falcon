#include "Protocol/Packets/AutomationClientConnectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AutomationClientConnectPacket::AutomationClientConnectPacket() = default;

void AutomationClientConnectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mAddress);
}

void AutomationClientConnectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAddress = stream.getString();
}

void AutomationClientConnectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

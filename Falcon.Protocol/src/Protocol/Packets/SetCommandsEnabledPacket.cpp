#include "Protocol/Packets/SetCommandsEnabledPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetCommandsEnabledPacket::SetCommandsEnabledPacket()
        : mCommandsEnabled(false) {}

void SetCommandsEnabledPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mCommandsEnabled);
}

void SetCommandsEnabledPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCommandsEnabled = stream.getBool();
}

void SetCommandsEnabledPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

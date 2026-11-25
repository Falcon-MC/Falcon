#include "Protocol/Packets/MultiplayerSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

MultiplayerSettingsPacket::MultiplayerSettingsPacket()
        : mMode(Mode::EnableMultiplayer) {}

void MultiplayerSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mMode);
}

void MultiplayerSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mMode = (Mode) stream.getVarInt();
}

void MultiplayerSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

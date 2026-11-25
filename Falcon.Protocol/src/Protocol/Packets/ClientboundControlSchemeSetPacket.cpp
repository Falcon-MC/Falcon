#include "Protocol/Packets/ClientboundControlSchemeSetPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundControlSchemeSetPacket::ClientboundControlSchemeSetPacket() = default;

void ClientboundControlSchemeSetPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mScheme);
}

void ClientboundControlSchemeSetPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mScheme = (ControlScheme) stream.getByte();
}

void ClientboundControlSchemeSetPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

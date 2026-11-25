#include "Protocol/Packets/ClientToServerHandshakePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientToServerHandshakePacket::ClientToServerHandshakePacket() {}

void ClientToServerHandshakePacket::write(BinaryStream &, const PacketCodecContext &) const {
}

void ClientToServerHandshakePacket::read(ReadOnlyBinaryStream &, const PacketCodecContext &) {
}

void ClientToServerHandshakePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

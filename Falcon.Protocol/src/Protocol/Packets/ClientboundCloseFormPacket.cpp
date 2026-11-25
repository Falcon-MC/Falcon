#include "Protocol/Packets/ClientboundCloseFormPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundCloseFormPacket::ClientboundCloseFormPacket() {}

void ClientboundCloseFormPacket::write(BinaryStream &, const PacketCodecContext &) const {
}

void ClientboundCloseFormPacket::read(ReadOnlyBinaryStream &, const PacketCodecContext &) {
}

void ClientboundCloseFormPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

#include "Protocol/Packets/ClientboundDataDrivenUIReloadPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundDataDrivenUIReloadPacket::ClientboundDataDrivenUIReloadPacket() = default;

void ClientboundDataDrivenUIReloadPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
}

void ClientboundDataDrivenUIReloadPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
}

void ClientboundDataDrivenUIReloadPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

#include "Protocol/Packets/ResourcePacksReadyForValidationPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ResourcePacksReadyForValidationPacket::ResourcePacksReadyForValidationPacket() = default;

void ResourcePacksReadyForValidationPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
}

void ResourcePacksReadyForValidationPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
}

void ResourcePacksReadyForValidationPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

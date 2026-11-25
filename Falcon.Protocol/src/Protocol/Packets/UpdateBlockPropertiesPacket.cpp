#include "Protocol/Packets/UpdateBlockPropertiesPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

UpdateBlockPropertiesPacket::UpdateBlockPropertiesPacket()
        : mProperties(Tag::ofCompound()) {}

void UpdateBlockPropertiesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    NbtIo::writeTag(stream, mProperties, NbtVariant::Network);
}

void UpdateBlockPropertiesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mProperties = NbtIo::readTag(stream, NbtVariant::Network);
}

void UpdateBlockPropertiesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

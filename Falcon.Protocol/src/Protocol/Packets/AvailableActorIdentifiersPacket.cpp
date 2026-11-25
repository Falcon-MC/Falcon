#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

AvailableActorIdentifiersPacket::AvailableActorIdentifiersPacket() = default;

void AvailableActorIdentifiersPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    NbtIo::writeTag(stream, mIdentifiers, NbtVariant::Network);
}

void AvailableActorIdentifiersPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mIdentifiers = NbtIo::readTag(stream, NbtVariant::Network);
}

void AvailableActorIdentifiersPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

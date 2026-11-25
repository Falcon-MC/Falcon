#include "Protocol/Packets/SyncActorPropertyPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

SyncActorPropertyPacket::SyncActorPropertyPacket()
        : mData(Tag::ofCompound()) {}

void SyncActorPropertyPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    NbtIo::writeTag(stream, mData, NbtVariant::Network);
}

void SyncActorPropertyPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mData = NbtIo::readTag(stream, NbtVariant::Network);
}

void SyncActorPropertyPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

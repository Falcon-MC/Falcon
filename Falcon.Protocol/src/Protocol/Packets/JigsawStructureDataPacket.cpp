#include "Protocol/Packets/JigsawStructureDataPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

JigsawStructureDataPacket::JigsawStructureDataPacket()
        : mJigsawStructureData(Tag::ofCompound()) {}

void JigsawStructureDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    NbtIo::writeTag(stream, mJigsawStructureData, NbtVariant::Network);
}

void JigsawStructureDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mJigsawStructureData = NbtIo::readTag(stream, NbtVariant::Network);
}

void JigsawStructureDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

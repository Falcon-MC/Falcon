#include "Protocol/Packets/BlockActorDataPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

BlockActorDataPacket::BlockActorDataPacket()
        : mData(Tag::ofCompound()) {}

void BlockActorDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    NbtIo::writeTag(stream, mData, NbtVariant::Network);
}

void BlockActorDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mData = NbtIo::readTag(stream, NbtVariant::Network);

    if (!mData.isCompound())
        throw BinaryDataException("Expected a compound tag in BlockActorDataPacket");
}

void BlockActorDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

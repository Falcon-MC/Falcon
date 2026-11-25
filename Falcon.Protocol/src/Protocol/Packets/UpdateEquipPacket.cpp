#include "Protocol/Packets/UpdateEquipPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

UpdateEquipPacket::UpdateEquipPacket()
        : mTag(Tag::ofCompound()) {}

void UpdateEquipPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mWindowId);
    stream.putByte((unsigned char) mWindowType);
    stream.putVarInt(mSize);
    stream.putVarLong(mUniqueActorId);
    NbtIo::writeTag(stream, mTag, NbtVariant::Network);
}

void UpdateEquipPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWindowId = stream.getByte();
    mWindowType = stream.getByte();
    mSize = stream.getVarInt();
    mUniqueActorId = stream.getVarLong();
    mTag = NbtIo::readTag(stream, NbtVariant::Network);
}

void UpdateEquipPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

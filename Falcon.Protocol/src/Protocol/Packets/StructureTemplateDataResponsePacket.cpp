#include "Protocol/Packets/StructureTemplateDataResponsePacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

StructureTemplateDataResponsePacket::StructureTemplateDataResponsePacket()
        : mSave(false), mTag(Tag::ofCompound()), mType(Type::None) {}

void StructureTemplateDataResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mName);
    stream.putBool(mSave);

    if (mSave)
        NbtIo::writeTag(stream, mTag, NbtVariant::Network);

    stream.putByte((unsigned char) mType);
}

void StructureTemplateDataResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mName = stream.getString();
    mSave = stream.getBool();

    if (mSave) {
        mTag = NbtIo::readTag(stream, NbtVariant::Network);

        if (!mTag.isCompound())
            throw BinaryDataException("Expected a compound tag in StructureTemplateDataResponsePacket");
    }

    mType = (Type) stream.getByte();
}

void StructureTemplateDataResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

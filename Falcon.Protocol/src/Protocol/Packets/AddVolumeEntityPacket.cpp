#include "Protocol/Packets/AddVolumeEntityPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

AddVolumeEntityPacket::AddVolumeEntityPacket() : mData(Tag::ofCompound()) {}

void AddVolumeEntityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt(mId);
    NbtIo::writeTag(stream, mData, NbtVariant::Network);
    stream.putString(mIdentifier);
    stream.putString(mInstanceName);
    stream.putBlockPosition(mMinBounds);
    stream.putBlockPosition(mMaxBounds);
    stream.putVarInt(mDimension);
    stream.putString(mEngineVersion);
}

void AddVolumeEntityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mId = stream.getUnsignedVarInt();
    mData = NbtIo::readTag(stream, NbtVariant::Network);
    mIdentifier = stream.getString();
    mInstanceName = stream.getString();
    mMinBounds = stream.getBlockPosition();
    mMaxBounds = stream.getBlockPosition();
    mDimension = stream.getVarInt();
    mEngineVersion = stream.getString();
}

void AddVolumeEntityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

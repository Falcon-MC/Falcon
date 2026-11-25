#include "Protocol/Packets/ItemRegistryPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

ItemRegistryPacket::ItemRegistryPacket() = default;

void ItemRegistryPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mEntries.size());
    for (const ItemComponentEntry &entry: mEntries) {
        stream.putString(entry.mIdentifier);
        stream.putLShort((uint16_t) entry.mRuntimeId);
        stream.putBool(entry.mComponentBased);
        stream.putVarInt(entry.mItemVersion);
        NbtIo::writeTag(stream, entry.mComponentData, NbtVariant::Network);
    }
}

void ItemRegistryPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mEntries.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        ItemComponentEntry entry;
        entry.mIdentifier = stream.getString();
        entry.mRuntimeId = (int16_t) stream.getLShort();
        entry.mComponentBased = stream.getBool();
        entry.mItemVersion = stream.getVarInt();
        entry.mComponentData = NbtIo::readTag(stream, NbtVariant::Network);
        mEntries.push_back(std::move(entry));
    }
}

void ItemRegistryPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

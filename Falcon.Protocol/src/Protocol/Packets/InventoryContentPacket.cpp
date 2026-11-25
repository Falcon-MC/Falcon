#include "Protocol/Packets/InventoryContentPacket.h"

#include "Protocol/InventoryCodec.h"
#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

InventoryContentPacket::InventoryContentPacket() = default;

void InventoryContentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mContainerId);

    stream.putArrayLength((uint32_t) mContents.size());
    for (const ItemStack &item: mContents) {
        ItemCodec::writeNetworkItemStackDescriptor(stream, context, item);
    }

    InventoryCodec::writeFullContainerName(stream, mContainerNameData);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mStorageItem);
}

void InventoryContentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = (int32_t) stream.getUnsignedVarInt();

    uint32_t length = stream.getArrayLength();
    mContents.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        mContents.push_back(ItemCodec::readNetworkItemStackDescriptor(stream, context));
    }

    mContainerNameData = InventoryCodec::readFullContainerName(stream);
    mStorageItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
}

void InventoryContentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

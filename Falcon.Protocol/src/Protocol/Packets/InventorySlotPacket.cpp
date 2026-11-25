#include "Protocol/Packets/InventorySlotPacket.h"

#include "Protocol/InventoryCodec.h"
#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

InventorySlotPacket::InventorySlotPacket() = default;

void InventorySlotPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mContainerId);
    stream.putUnsignedVarInt((uint32_t) mSlot);

    stream.putOptionalPresent(mHasContainerNameData);
    if (mHasContainerNameData) {
        InventoryCodec::writeFullContainerName(stream, mContainerNameData);
    }

    stream.putOptionalPresent(mHasStorageItem);
    if (mHasStorageItem) {
        ItemCodec::writeNetworkItemStackDescriptor(stream, context, mStorageItem);
    }

    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItem);
}

void InventorySlotPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = (int32_t) stream.getUnsignedVarInt();
    mSlot = (int32_t) stream.getUnsignedVarInt();

    mHasContainerNameData = stream.getOptionalPresent();
    if (mHasContainerNameData) {
        mContainerNameData = InventoryCodec::readFullContainerName(stream);
    }

    mHasStorageItem = stream.getOptionalPresent();
    if (mHasStorageItem) {
        mStorageItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    }

    mItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
}

void InventorySlotPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

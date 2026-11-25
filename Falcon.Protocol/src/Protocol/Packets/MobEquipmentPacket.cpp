#include "Protocol/Packets/MobEquipmentPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

MobEquipmentPacket::MobEquipmentPacket() = default;

void MobEquipmentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItem);
    stream.putByte((unsigned char) mInventorySlot);
    stream.putByte((unsigned char) mHotbarSlot);
    stream.putByte((unsigned char) mContainerId);
}

void MobEquipmentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mInventorySlot = stream.getByte();
    mHotbarSlot = stream.getByte();
    mContainerId = stream.getSignedByte();
}

void MobEquipmentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

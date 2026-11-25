#include "Protocol/Packets/MobArmorEquipmentPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

MobArmorEquipmentPacket::MobArmorEquipmentPacket() = default;

void MobArmorEquipmentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mHelmet);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mChestplate);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mLeggings);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mBoots);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mBody);
}

void MobArmorEquipmentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mHelmet = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mChestplate = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mLeggings = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mBoots = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mBody = ItemCodec::readNetworkItemStackDescriptor(stream, context);
}

void MobArmorEquipmentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

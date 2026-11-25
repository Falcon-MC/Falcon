#include "Protocol/Packets/VirtualContainerOpenPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

VirtualContainerOpenPacket::VirtualContainerOpenPacket() = default;

void VirtualContainerOpenPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte(mContainerId);
    stream.putUnsignedVarLong(mSessionId);
    stream.putString(mTitle);

    stream.putArrayLength((uint32_t) mSlots.size());
    for (const ItemStack &item: mSlots)
        ItemCodec::writeNetworkItemStackDescriptor(stream, context, item);
}

void VirtualContainerOpenPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = stream.getByte();
    mSessionId = stream.getUnsignedVarLong();
    mTitle = stream.getString();

    const uint32_t length = stream.getArrayLength();
    mSlots.clear();
    mSlots.reserve(length);
    for (uint32_t i = 0; i < length; i++)
        mSlots.push_back(ItemCodec::readNetworkItemStackDescriptor(stream, context));
}

void VirtualContainerOpenPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

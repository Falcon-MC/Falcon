#include "Protocol/Packets/UpdateBlockPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateBlockPacket::UpdateBlockPacket()
        : mRuntimeId(0), mFlags(Flag::All), mDataLayer(0) {}

void UpdateBlockPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    stream.putUnsignedVarInt((uint32_t) context.toNetworkBlockId((int32_t) mRuntimeId));
    stream.putUnsignedVarInt(mFlags);
    stream.putUnsignedVarInt(mDataLayer);
}

void UpdateBlockPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mRuntimeId = (uint32_t) context.fromNetworkBlockId((int32_t) stream.getUnsignedVarInt());
    mFlags = stream.getUnsignedVarInt();
    mDataLayer = stream.getUnsignedVarInt();
}

void UpdateBlockPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

#include "Protocol/Packets/UpdateBlockSyncedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateBlockSyncedPacket::UpdateBlockSyncedPacket()
        : mRuntimeId(0), mFlags(UpdateBlockPacket::Flag::All), mDataLayer(0) {}

void UpdateBlockSyncedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    stream.putUnsignedVarInt((uint32_t) context.toNetworkBlockId((int32_t) mRuntimeId));
    stream.putUnsignedVarInt(mFlags);
    stream.putUnsignedVarInt(mDataLayer);
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putUnsignedVarLong((uint64_t) mEntityBlockSyncType);
}

void UpdateBlockSyncedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mRuntimeId = (uint32_t) context.fromNetworkBlockId((int32_t) stream.getUnsignedVarInt());
    mFlags = stream.getUnsignedVarInt();
    mDataLayer = stream.getUnsignedVarInt();
    mRuntimeActorId = stream.getUnsignedVarLong();
    mEntityBlockSyncType = (BlockSyncType) stream.getUnsignedVarLong();
}

void UpdateBlockSyncedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

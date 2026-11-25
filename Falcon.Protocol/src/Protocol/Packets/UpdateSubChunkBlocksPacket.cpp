#include "Protocol/Packets/UpdateSubChunkBlocksPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeBlockChangeEntry(BinaryStream &stream, const PacketCodecContext &context,
                               const BlockChangeEntry &entry) {
        stream.putBlockPosition(entry.mPosition);
        stream.putUnsignedVarInt((uint32_t) context.toNetworkBlockId((int32_t) entry.mRuntimeId));
        stream.putUnsignedVarInt(entry.mUpdateFlags);
        stream.putUnsignedVarLong(entry.mMessageEntityId);
        stream.putUnsignedVarInt((uint32_t) entry.mMessageType);
    }

    BlockChangeEntry readBlockChangeEntry(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
        BlockChangeEntry entry;
        entry.mPosition = stream.getBlockPosition();
        entry.mRuntimeId = (uint32_t) context.fromNetworkBlockId((int32_t) stream.getUnsignedVarInt());
        entry.mUpdateFlags = stream.getUnsignedVarInt();
        entry.mMessageEntityId = stream.getUnsignedVarLong();
        entry.mMessageType = (BlockChangeMessageType) stream.getUnsignedVarInt();
        return entry;
    }

}

UpdateSubChunkBlocksPacket::UpdateSubChunkBlocksPacket() = default;

void UpdateSubChunkBlocksPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mPosition);

    stream.putArrayLength((uint32_t) mStandardBlocks.size());
    for (const BlockChangeEntry &entry: mStandardBlocks) {
        writeBlockChangeEntry(stream, context, entry);
    }

    stream.putArrayLength((uint32_t) mExtraBlocks.size());
    for (const BlockChangeEntry &entry: mExtraBlocks) {
        writeBlockChangeEntry(stream, context, entry);
    }
}

void UpdateSubChunkBlocksPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPosition = stream.getBlockPosition();

    uint32_t standardCount = stream.getArrayLength();
    mStandardBlocks.reserve(standardCount);
    for (uint32_t i = 0; i < standardCount; i++) {
        mStandardBlocks.push_back(readBlockChangeEntry(stream, context));
    }

    uint32_t extraCount = stream.getArrayLength();
    mExtraBlocks.reserve(extraCount);
    for (uint32_t i = 0; i < extraCount; i++) {
        mExtraBlocks.push_back(readBlockChangeEntry(stream, context));
    }
}

void UpdateSubChunkBlocksPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

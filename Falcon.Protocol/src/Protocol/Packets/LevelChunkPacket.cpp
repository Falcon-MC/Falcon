#include "Protocol/Packets/LevelChunkPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LevelChunkPacket::LevelChunkPacket()
        : mChunkX(0), mChunkZ(0), mDimension(0), mSubChunksLength(0), mCachingEnabled(false),
          mRequestSubChunks(false), mSubChunkLimit(0), mClientBiomeUpdate(false) {}

void LevelChunkPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mChunkX);
    stream.putVarInt(mChunkZ);
    stream.putVarInt(mDimension);
    stream.putUnsignedVarInt(mSubChunksLength);

    stream.putOptionalPresent(mRequestSubChunks);
    if (mRequestSubChunks)
        stream.putVarInt(mSubChunkLimit);

    stream.putBool(mCachingEnabled);

    stream.putUnsignedVarInt((uint32_t) mBlobIds.size());
    for (uint64_t blobId: mBlobIds)
        stream.putLLong(blobId);

    stream.putByteArray(mData);
    if (context.getCapabilities().mLevelChunkBiomeUpdate)
        stream.putBool(mClientBiomeUpdate);
}

void LevelChunkPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mChunkX = stream.getVarInt();
    mChunkZ = stream.getVarInt();
    mDimension = stream.getVarInt();
    mSubChunksLength = stream.getUnsignedVarInt();

    mRequestSubChunks = stream.getOptionalPresent();
    mSubChunkLimit = mRequestSubChunks ? stream.getVarInt() : 0;

    mCachingEnabled = stream.getBool();

    const uint32_t blobCount = stream.getArrayLength();
    mBlobIds.clear();
    mBlobIds.reserve(blobCount);

    for (uint32_t i = 0; i < blobCount; i++)
        mBlobIds.push_back(stream.getLLong());

    mData = stream.getByteArray();
    mClientBiomeUpdate = context.getCapabilities().mLevelChunkBiomeUpdate && stream.getBool();
}

void LevelChunkPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

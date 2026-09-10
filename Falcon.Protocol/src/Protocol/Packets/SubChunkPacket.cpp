#include "Protocol/Packets/SubChunkPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    // The height map is a fixed size grid, so a partially filled buffer would desync the stream
    void writeHeightMap(BinaryStream &stream, HeightMapDataType type, const std::string &data) {
        stream.putByte((unsigned char) type);
        stream.putOptionalPresent(!data.empty());

        if (data.empty())
            return;

        if (data.size() != SubChunkData::HEIGHT_MAP_LENGTH) {
            throw BinaryDataException("Height map must be exactly " +
                                      std::to_string(SubChunkData::HEIGHT_MAP_LENGTH) + " bytes");
        }

        for (size_t offset = 0; offset < SubChunkData::HEIGHT_MAP_LENGTH;
             offset += SubChunkData::HEIGHT_MAP_RUN_LENGTH) {
            stream.putUnsignedVarInt((uint32_t) SubChunkData::HEIGHT_MAP_RUN_LENGTH);
            stream.put(data.data() + offset, SubChunkData::HEIGHT_MAP_RUN_LENGTH);
        }
    }

    std::string readHeightMap(ReadOnlyBinaryStream &stream) {
        std::string data;
        data.reserve(SubChunkData::HEIGHT_MAP_LENGTH);

        for (size_t offset = 0; offset < SubChunkData::HEIGHT_MAP_LENGTH;
             offset += SubChunkData::HEIGHT_MAP_RUN_LENGTH) {
            const uint32_t runLength = stream.getUnsignedVarInt();
            if (runLength != SubChunkData::HEIGHT_MAP_RUN_LENGTH) {
                throw BinaryDataException("Height map run must be exactly " +
                                          std::to_string(SubChunkData::HEIGHT_MAP_RUN_LENGTH) + " bytes");
            }
            data += stream.get(runLength);
        }

        return data;
    }

    void writeSubChunk(BinaryStream &stream, const SubChunkData &subChunk) {
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.x);
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.y);
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.z);
        stream.putByte((unsigned char) subChunk.mResult);

        stream.putOptionalPresent(subChunk.mHasData);
        if (subChunk.mHasData)
            stream.putByteArray(subChunk.mData);

        writeHeightMap(stream, subChunk.mHeightMapType, subChunk.mHeightMapData);
        writeHeightMap(stream, subChunk.mRenderHeightMapType, subChunk.mRenderHeightMapData);

        stream.putOptionalPresent(subChunk.mHasBlobId);
        if (subChunk.mHasBlobId)
            stream.putLLong(subChunk.mBlobId);
    }

    SubChunkData readSubChunk(ReadOnlyBinaryStream &stream) {
        SubChunkData subChunk;

        const int8_t offsetX = (int8_t) stream.getByte();
        const int8_t offsetY = (int8_t) stream.getByte();
        const int8_t offsetZ = (int8_t) stream.getByte();
        subChunk.mPosition = Vector3i(offsetX, offsetY, offsetZ);
        subChunk.mResult = (SubChunkRequestResult) stream.getByte();

        subChunk.mHasData = stream.getOptionalPresent();
        if (subChunk.mHasData)
            subChunk.mData = stream.getByteArray();

        subChunk.mHeightMapType = (HeightMapDataType) stream.getByte();
        if (stream.getOptionalPresent())
            subChunk.mHeightMapData = readHeightMap(stream);

        subChunk.mRenderHeightMapType = (HeightMapDataType) stream.getByte();
        if (stream.getOptionalPresent())
            subChunk.mRenderHeightMapData = readHeightMap(stream);

        subChunk.mHasBlobId = stream.getOptionalPresent();
        if (subChunk.mHasBlobId)
            subChunk.mBlobId = stream.getLLong();

        return subChunk;
    }

}

SubChunkPacket::SubChunkPacket()
        : mCacheEnabled(false), mDimension(0) {}

void SubChunkPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mCacheEnabled);
    stream.putVarInt(mDimension);
    stream.putLInt((uint32_t) mCenterPosition.x);
    stream.putLInt((uint32_t) mCenterPosition.y);
    stream.putLInt((uint32_t) mCenterPosition.z);

    stream.putArrayLength((uint32_t) mSubChunks.size());
    for (const SubChunkData &subChunk: mSubChunks)
        writeSubChunk(stream, subChunk);
}

void SubChunkPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCacheEnabled = stream.getBool();
    mDimension = stream.getVarInt();
    const int32_t centerX = stream.getSignedLInt();
    const int32_t centerY = stream.getSignedLInt();
    const int32_t centerZ = stream.getSignedLInt();
    mCenterPosition = Vector3i(centerX, centerY, centerZ);

    const uint32_t count = stream.getArrayLength();
    mSubChunks.clear();
    mSubChunks.reserve(count);

    for (uint32_t i = 0; i < count; i++)
        mSubChunks.push_back(readSubChunk(stream));
}

void SubChunkPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

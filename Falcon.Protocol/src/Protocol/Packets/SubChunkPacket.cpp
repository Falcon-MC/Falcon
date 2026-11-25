#include "Protocol/Packets/SubChunkPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    // The height map is a fixed size grid, so a partially filled buffer would desync the stream
    void writeHeightMap(BinaryStream &stream, HeightMapDataType type, const std::string &data, bool rows) {
        stream.putByte((unsigned char) type);
        stream.putOptionalPresent(!data.empty());

        if (data.empty())
            return;

        if (data.size() != SubChunkData::HEIGHT_MAP_LENGTH) {
            throw BinaryDataException("Height map must be exactly " +
                                      std::to_string(SubChunkData::HEIGHT_MAP_LENGTH) + " bytes");
        }

        if (!rows) {
            stream.put(data.data(), SubChunkData::HEIGHT_MAP_LENGTH);
            return;
        }

        for (size_t offset = 0; offset < SubChunkData::HEIGHT_MAP_LENGTH;
             offset += SubChunkData::HEIGHT_MAP_RUN_LENGTH) {
            stream.putUnsignedVarInt((uint32_t) SubChunkData::HEIGHT_MAP_RUN_LENGTH);
            stream.put(data.data() + offset, SubChunkData::HEIGHT_MAP_RUN_LENGTH);
        }
    }

    struct HeightMapLayout {
        bool optionalFlag;
        bool runs;
    };

    constexpr HeightMapLayout HEIGHT_MAP_LAYOUTS[] = {
            {true, true},
            {false, false},
            {true, false},
            {false, true},
    };

    std::string readHeightMapBody(ReadOnlyBinaryStream &stream, bool runs) {
        if (!runs)
            return stream.get(SubChunkData::HEIGHT_MAP_LENGTH);

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

    std::string readHeightMap(ReadOnlyBinaryStream &stream, HeightMapDataType type, const HeightMapLayout &layout) {
        const bool present = layout.optionalFlag ? stream.getOptionalPresent() : type == HeightMapDataType::HasData;
        if (!present)
            return {};
        return readHeightMapBody(stream, layout.runs);
    }

    void writeSubChunk(BinaryStream &stream, const SubChunkData &subChunk, bool heightMapRows) {
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.x);
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.y);
        stream.putByte((unsigned char) (int8_t) subChunk.mPosition.z);
        stream.putByte((unsigned char) subChunk.mResult);

        stream.putOptionalPresent(subChunk.mHasData);
        if (subChunk.mHasData)
            stream.putByteArray(subChunk.mData);

        writeHeightMap(stream, subChunk.mHeightMapType, subChunk.mHeightMapData, heightMapRows);
        writeHeightMap(stream, subChunk.mRenderHeightMapType, subChunk.mRenderHeightMapData, heightMapRows);

        stream.putOptionalPresent(subChunk.mHasBlobId);
        if (subChunk.mHasBlobId)
            stream.putLLong(subChunk.mBlobId);
    }

    SubChunkData readSubChunk(ReadOnlyBinaryStream &stream, const HeightMapLayout &layout) {
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
        subChunk.mHeightMapData = readHeightMap(stream, subChunk.mHeightMapType, layout);

        subChunk.mRenderHeightMapType = (HeightMapDataType) stream.getByte();
        subChunk.mRenderHeightMapData = readHeightMap(stream, subChunk.mRenderHeightMapType, layout);

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
        writeSubChunk(stream, subChunk, context.getCapabilities().mSubChunkHeightMapRows);
}

void SubChunkPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCacheEnabled = stream.getBool();
    mDimension = stream.getVarInt();
    const int32_t centerX = stream.getSignedLInt();
    const int32_t centerY = stream.getSignedLInt();
    const int32_t centerZ = stream.getSignedLInt();
    mCenterPosition = Vector3i(centerX, centerY, centerZ);

    const uint32_t count = stream.getArrayLength();
    const size_t start = stream.getOffset();

    for (size_t attempt = 0; attempt < std::size(HEIGHT_MAP_LAYOUTS); attempt++) {
        stream.setOffset(start);
        mSubChunks.clear();
        mSubChunks.reserve(count);
        try {
            for (uint32_t i = 0; i < count; i++)
                mSubChunks.push_back(readSubChunk(stream, HEIGHT_MAP_LAYOUTS[attempt]));
            if (stream.feof())
                return;
        } catch (const BinaryDataException &) {
            if (attempt + 1 == std::size(HEIGHT_MAP_LAYOUTS))
                throw;
        }
    }

    throw BinaryDataException("Sub chunk height maps do not match any known layout");
}

void SubChunkPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}

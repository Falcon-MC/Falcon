#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

enum class SubChunkRequestResult : int {
    Undefined = 0,
    Success = 1,
    ChunkNotFound = 2,
    InvalidDimension = 3,
    PlayerNotFound = 4,
    IndexOutOfBounds = 5,
    SuccessAllAir = 6
};

enum class HeightMapDataType : int {
    NoData = 0,
    HasData = 1,
    TooHigh = 2,
    TooLow = 3,
    Copied = 4
};

struct SubChunkData {
    static const size_t HEIGHT_MAP_LENGTH = 272;
    static const size_t HEIGHT_MAP_RUN_LENGTH = 16;

    Vector3i mPosition;
    SubChunkRequestResult mResult;

    bool mHasData;
    std::string mData;

    HeightMapDataType mHeightMapType;
    std::string mHeightMapData;

    HeightMapDataType mRenderHeightMapType;
    std::string mRenderHeightMapData;

    bool mHasBlobId;
    uint64_t mBlobId;

    SubChunkData()
            : mResult(SubChunkRequestResult::Undefined), mHasData(false), mHeightMapType(HeightMapDataType::NoData),
              mRenderHeightMapType(HeightMapDataType::NoData), mHasBlobId(false), mBlobId(0) {}
};

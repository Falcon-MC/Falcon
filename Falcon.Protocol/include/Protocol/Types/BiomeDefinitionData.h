#pragma once

#include "Protocol/Types/BiomeChunkGenData.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class BiomeDefinitionData {
public:
    std::string mName;
    uint16_t mNameIndex = 0;
    uint16_t mId = 0xFFFF;
    float mTemperature = 0.5f;
    float mDownfall = 0.5f;
    float mFoliageSnow = 0.0f;
    float mDepth = 0.1f;
    float mScale = 0.1f;
    int32_t mMapWaterColorArgb = (int32_t) 0xFF44AFF5;
    bool mRain = true;
    std::optional<std::vector<uint16_t>> mTagIndexes;
    std::optional<BiomeDefinitionChunkGenData> mChunkGenData;
};

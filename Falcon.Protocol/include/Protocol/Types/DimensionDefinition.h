#pragma once

#include "Core/Utility/UUID.h"

#include <cstdint>
#include <string>

class DimensionDefinition {
public:
    std::string mId;
    int32_t mMaximumHeight = 0;
    int32_t mMinimumHeight = 0;
    int32_t mGeneratorType = 0;
    int32_t mDimensionType = 0;
    Uuid mPackId;
    std::string mDefaultBiome;
    int32_t mCloudHeight = 0;
    bool mRenderClouds = false;
};

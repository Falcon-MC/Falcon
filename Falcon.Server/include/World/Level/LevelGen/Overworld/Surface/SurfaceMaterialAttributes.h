#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <vector>

class SurfaceMaterialAttributes {
public:
    const BlockState *mTopBlock = nullptr;
    const BlockState *mMidBlock = nullptr;
    const BlockState *mSeaFloorBlock = nullptr;
    int32_t mSeaFloorDepth = 0;
};

class SurfaceMaterialAdjustmentAttributes {
public:
    float mNoiseLowerBound = 0.0f;
    float mNoiseUpperBound = 0.0f;
    const BlockState *mTopBlock = nullptr;
    const BlockState *mMidBlock = nullptr;
    const BlockState *mSeaFloorBlock = nullptr;
};

class SurfaceBuilderComponent {
public:
    SurfaceMaterialAttributes mSurfaceMaterial;
    std::vector<SurfaceMaterialAdjustmentAttributes> mSurfaceMaterialAdjustments;

    static const SurfaceBuilderComponent *getForBiome(int32_t biomeId);
};

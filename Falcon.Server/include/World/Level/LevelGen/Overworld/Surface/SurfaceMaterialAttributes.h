#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <vector>

class SurfaceMaterialAttributes {
public:
    const BlockState *mTopBlock = nullptr;
    const BlockState *mMidBlock = nullptr;
    const BlockState *mSeaFloorBlock = nullptr;
    const BlockState *mFoundationBlock = nullptr;
    const BlockState *mSeaBlock = nullptr;
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

// the "type" of a biome's minecraft:surface_builder
enum class SurfaceBuilderType {
    Overworld,
    Mesa,
    Swamp,
    FrozenOcean
};

class SurfaceBuilderComponent {
public:
    SurfaceBuilderType mType = SurfaceBuilderType::Overworld;
    SurfaceMaterialAttributes mSurfaceMaterial;
    std::vector<SurfaceMaterialAdjustmentAttributes> mSurfaceMaterialAdjustments;

    static const SurfaceBuilderComponent *getForBiome(int32_t biomeId);
};

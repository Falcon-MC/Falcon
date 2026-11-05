#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Overworld/Terrain/LatticeNoise.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldAquifer.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <cstdint>
#include <vector>

class LevelChunk;
class OverworldClimate;

// stone, deepslate, ore veins and aquifer fluids from densities at cell corners (sub_14CDEE380, sub_14CDF0330)
class OverworldTerrainFiller {
public:
    OverworldTerrainFiller(const XoroshiroPositionalFactory &noiseFactory, const OverworldClimate &climate);

    // one per chunk, shared by the fill and the carvers
    OverworldAquifer createAquifer() const;

    // heightMap is indexed (x << 4) | z and receives the highest non-air y of each column
    void fill(LevelChunk &chunk, int32_t *heightMap, OverworldAquifer &aquifer) const;

private:
    // corners per chunk: 5 x 41 x 5, y varying fastest
    static constexpr int32_t CORNERS_XZ = 5;
    static constexpr int32_t CORNERS_Y = 41;
    static constexpr int32_t CORNER_COUNT = CORNERS_XZ * CORNERS_XZ * CORNERS_Y;

    struct Corners {
        std::vector<float> mDensity;
        std::vector<float> mNoodleToggle;
        std::vector<float> mNoodleThickness;
        std::vector<float> mNoodleRidgeA;
        std::vector<float> mNoodleRidgeB;
        std::vector<float> mVeinToggle;
        std::vector<float> mVeinRidgeA;
        std::vector<float> mVeinRidgeB;
    };

    void _sampleCorners(int32_t chunkX, int32_t chunkZ, Corners &corners) const;

    float _caves(float density, float x, float y, float z) const;

    const OverworldClimate &mClimate;
    std::vector<LatticeNoise> mMinLimitNoise;
    std::vector<LatticeNoise> mMaxLimitNoise;
    std::vector<LatticeNoise> mMainNoise;

    NormalNoisePtr mPillar;
    NormalNoisePtr mPillarRareness;
    NormalNoisePtr mPillarThickness;
    NormalNoisePtr mSpaghetti2d;
    NormalNoisePtr mSpaghetti2dElevation;
    NormalNoisePtr mSpaghetti2dModulator;
    NormalNoisePtr mSpaghetti2dThickness;
    NormalNoisePtr mSpaghetti3dFirst;
    NormalNoisePtr mSpaghetti3dSecond;
    NormalNoisePtr mSpaghetti3dRarity;
    NormalNoisePtr mSpaghetti3dThickness;
    NormalNoisePtr mSpaghettiRoughness;
    NormalNoisePtr mSpaghettiRoughnessModulator;
    NormalNoisePtr mCaveEntrance;
    NormalNoisePtr mCaveLayer;
    NormalNoisePtr mCaveCheese;

    NormalNoisePtr mNoodle;
    NormalNoisePtr mNoodleThickness;
    NormalNoisePtr mNoodleRidgeA;
    NormalNoisePtr mNoodleRidgeB;

    NormalNoisePtr mVeininess;
    NormalNoisePtr mVeinA;
    NormalNoisePtr mVeinB;
    NormalNoisePtr mVeinGap;
    XoroshiroPositionalFactory mVeinFactory;
    XoroshiroPositionalFactory mDeepslateFactory;

    OverworldAquiferNoises mAquiferNoises;
};

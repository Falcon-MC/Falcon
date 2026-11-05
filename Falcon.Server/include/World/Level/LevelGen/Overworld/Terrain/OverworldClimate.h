#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainSpline.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <cstdint>

// climate and terrain shape per quart column, like the game's noise cache (sub_14CDED4D0)
class OverworldClimate {
public:
    struct Column {
        float mShiftedX;
        float mShiftedZ;
        float mContinentalness;
        float mRidges;
        float mErosion;
        float mOffset;
        float mFactor;
        float mJaggedness;
        float mTemperature;
        float mVegetation;
    };

    static constexpr int32_t NO_SURFACE = 0x7FFF;

    explicit OverworldClimate(const XoroshiroPositionalFactory &noiseFactory);

    Column sampleColumn(int32_t quartX, int32_t quartZ) const;

    // highest cell bottom whose initial density is solid, NO_SURFACE when none (sub_14CDEF730)
    int32_t preliminarySurfaceLevel(int32_t quartX, int32_t quartZ) const;

    // terrain constant included
    float offsetAt(int32_t quartX, int32_t quartZ, float *erosion) const;

    static float foldRidges(float ridges);

private:
    struct Shift {
        float mX;
        float mZ;
    };

    Shift _shift(int32_t quartX, int32_t quartZ) const;

    TerrainSpline::Climate _climate(const Shift &shift) const;

    NormalNoisePtr mTemperature;
    NormalNoisePtr mVegetation;
    NormalNoisePtr mContinentalness;
    NormalNoisePtr mErosion;
    NormalNoisePtr mRidges;
    NormalNoisePtr mShift;
    NormalNoisePtr mJagged;
    TerrainSplinePtr mOffsetSpline;
    TerrainSplinePtr mFactorSpline;
    TerrainSplinePtr mJaggednessSpline;
};

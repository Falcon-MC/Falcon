#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"

#include <atomic>
#include <cmath>

namespace {

    // float 0xBF00F5C3 and 128 next to the splines (sub_14718F810)
    constexpr float OFFSET_BASE = -0.50375f;
    constexpr float FACTOR_SCALE = 128.0f;
    constexpr float JAGGED_SCALE = 1500.0f;

    // initial density of the preliminary surface, positive part scaled by 4 (qword_14F49A628)
    float quarterNegative(float density) {
        return density * (density > 0.0f ? 4.0f : 1.0f);
    }

}

OverworldClimate::OverworldClimate(const XoroshiroPositionalFactory &noiseFactory) {
    static std::atomic<uint64_t> nextId{1};
    mId = nextId.fetch_add(1);

    mTemperature = NormalNoise::create(noiseFactory, "minecraft:temperature", -10,
                                       {1.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mVegetation = NormalNoise::create(noiseFactory, "minecraft:vegetation", -8, {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    mContinentalness = NormalNoise::create(noiseFactory, "minecraft:continentalness", -9,
                                           {1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    mErosion = NormalNoise::create(noiseFactory, "minecraft:erosion", -9, {1.0f, 1.0f, 0.0f, 1.0f, 1.0f});
    mRidges = NormalNoise::create(noiseFactory, "minecraft:ridge", -7, {1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mShift = NormalNoise::create(noiseFactory, "minecraft:offset", -3, {1.0f, 1.0f, 1.0f, 0.0f});
    mJagged = NormalNoise::create(noiseFactory, "minecraft:jagged", -16,
                                  {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f});
    mOffsetSpline = TerrainSpline::overworldOffset();
    mFactorSpline = TerrainSpline::overworldFactor();
    mJaggednessSpline = TerrainSpline::overworldJaggedness();
}

float OverworldClimate::foldRidges(float ridges) {
    return (std::fabs(std::fabs(ridges) + -0.6666667f) + -0.33333334f) * -3.0f;
}

OverworldClimate::Shift OverworldClimate::_shift(int32_t quartX, int32_t quartZ) const {
    const float x = static_cast<float>(quartX);
    const float z = static_cast<float>(quartZ);
    const float shiftX = mShift->getValue(x, 0.0f, z) * 4.0f;
    const float shiftZ = mShift->getValue(z, x, 0.0f);
    return Shift{shiftX + x, shiftZ * 4.0f + z};
}

TerrainSpline::Climate OverworldClimate::_climate(const Shift &shift) const {
    TerrainSpline::Climate climate{};
    climate.mContinents = mContinentalness->getValue(shift.mX, 0.0f, shift.mZ);
    climate.mErosion = mErosion->getValue(shift.mX, 0.0f, shift.mZ);
    climate.mRidges = mRidges->getValue(shift.mX, 0.0f, shift.mZ);
    climate.mRidgesFolded = foldRidges(climate.mRidges);
    return climate;
}

OverworldClimate::Column OverworldClimate::sampleColumn(int32_t quartX, int32_t quartZ) const {
    const Shift shift = _shift(quartX, quartZ);
    const TerrainSpline::Climate climate = _climate(shift);

    Column column{};
    column.mShiftedX = shift.mX;
    column.mShiftedZ = shift.mZ;
    column.mContinentalness = climate.mContinents;
    column.mRidges = climate.mRidges;
    column.mErosion = climate.mErosion;
    column.mOffset = mOffsetSpline->apply(climate) + OFFSET_BASE;
    column.mFactor = mFactorSpline->apply(climate) * FACTOR_SCALE;

    const float jaggedness = mJaggednessSpline->apply(climate);
    column.mJaggedness = 0.0f;
    if (jaggedness > 0.0f) {
        const float jagged = mJagged->getValue(static_cast<float>(quartX) * 4.0f * JAGGED_SCALE, 0.0f,
                                               static_cast<float>(quartZ) * 4.0f * JAGGED_SCALE);
        column.mJaggedness = (jagged > 0.0f ? jaggedness : jaggedness * 0.5f) * jagged;
    }

    column.mTemperature = mTemperature->getValue(shift.mX, 0.0f, shift.mZ);
    column.mVegetation = mVegetation->getValue(shift.mX, 0.0f, shift.mZ);
    return column;
}

float OverworldClimate::offsetAt(int32_t quartX, int32_t quartZ, float *erosion) const {
    const TerrainSpline::Climate climate = _climate(_shift(quartX, quartZ));
    *erosion = climate.mErosion;
    return mOffsetSpline->apply(climate) + OFFSET_BASE;
}

int32_t OverworldClimate::preliminarySurfaceLevel(int32_t quartX, int32_t quartZ) const {
    // The terrain filler, the aquifer, the surface builder and feature placement all ask for the same few columns
    // of a chunk, often per block; the level is a pure function of the column, so each worker remembers recent ones.
    struct Remembered {
        uint64_t mClimate = 0;
        int32_t mQuartX = 0;
        int32_t mQuartZ = 0;
        int32_t mLevel = 0;
    };
    thread_local Remembered remembered[1024];

    const uint32_t slot = ((uint32_t) quartX * 73856093u ^ (uint32_t) quartZ * 19349663u) & 1023u;
    Remembered &entry = remembered[slot];
    if (entry.mClimate == mId && entry.mQuartX == quartX && entry.mQuartZ == quartZ)
        return entry.mLevel;

    entry.mClimate = mId;
    entry.mQuartX = quartX;
    entry.mQuartZ = quartZ;
    entry.mLevel = _computePreliminarySurfaceLevel(quartX, quartZ);
    return entry.mLevel;
}

int32_t OverworldClimate::_computePreliminarySurfaceLevel(int32_t quartX, int32_t quartZ) const {
    const TerrainSpline::Climate climate = _climate(_shift(quartX, quartZ));
    const float offset = mOffsetSpline->apply(climate) + OFFSET_BASE;
    const float factor = mFactorSpline->apply(climate) * FACTOR_SCALE;

    for (int32_t cell = 39; cell >= 0; cell--) {
        const int32_t y = 8 * (cell - 8);
        float density = (static_cast<float>(y) * -0.0078125f + 1.0f + offset) * factor;
        density = quarterNegative(density) + -90.0f;

        const float topSlide = static_cast<float>(40 - cell) * 0.5f;
        float slid = -10.0f;
        if (topSlide >= 0.0f)
            slid = topSlide <= 1.0f ? topSlide * (density + 10.0f) + -10.0f : density;

        const float bottomSlide = static_cast<float>(cell) / 3.0f;
        const float result = bottomSlide > 1.0f ? slid : (slid + -15.0f) * bottomSlide + 15.0f;
        if (result > 50.0f)
            return y;
    }

    return NO_SURFACE;
}

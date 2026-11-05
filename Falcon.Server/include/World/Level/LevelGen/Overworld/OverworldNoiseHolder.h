#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <memory>

class OverworldBiomeNoises {
public:
    explicit OverworldBiomeNoises(const XoroshiroPositionalFactory &noiseFactory);

    const NormalNoisePtr &getContinentalNoise() const { return mContinentalNoise; }

    const NormalNoisePtr &getTemperatureNoise() const { return mTemperatureNoise; }

    const NormalNoisePtr &getHumidityNoise() const { return mHumidityNoise; }

    const NormalNoisePtr &getErosionNoise() const { return mErosionNoise; }

    const NormalNoisePtr &getWeirdnessNoise() const { return mWeirdnessNoise; }

    const NormalNoisePtr &getOffsetNoise() const { return mOffsetNoise; }

private:
    NormalNoisePtr mContinentalNoise;
    NormalNoisePtr mTemperatureNoise;
    NormalNoisePtr mHumidityNoise;
    NormalNoisePtr mErosionNoise;
    NormalNoisePtr mWeirdnessNoise;
    NormalNoisePtr mOffsetNoise;
};

class OverworldTerrainNoises {
public:
    explicit OverworldTerrainNoises(const OverworldBiomeNoises &biomeNoises);

    const DensityFunctionPtr &getContinents() const { return mContinents; }

    const DensityFunctionPtr &getErosion() const { return mErosion; }

    const DensityFunctionPtr &getRidges() const { return mRidges; }

private:
    DensityFunctionPtr mContinents;
    DensityFunctionPtr mErosion;
    DensityFunctionPtr mRidges;
};

class OverworldNoiseHolder {
public:
    explicit OverworldNoiseHolder(int64_t seed);

    const OverworldBiomeNoises &getBiomeNoises() const { return *mBiomeNoises; }

    const OverworldTerrainNoises &getTerrainNoises() const { return *mTerrainNoises; }

    IRandom &getRandomSource() const { return *mRandomSource; }

    const XoroshiroPositionalFactory &getNoiseFactory() const {
        return mNoiseFactory;
    }

private:
    IRandomPtr mRandomSource;
    XoroshiroPositionalFactory mNoiseFactory;
    std::unique_ptr<OverworldBiomeNoises> mBiomeNoises;
    std::unique_ptr<OverworldTerrainNoises> mTerrainNoises;
};

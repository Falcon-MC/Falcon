#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>

class NetherTerrainNoises {
public:
    explicit NetherTerrainNoises(IRandom &random);

    const NormalNoisePtr &getSurfaceNoise() const { return mSurfaceNoise; }

    const NormalNoisePtr &getPatchNoise() const { return mPatchNoise; }

    const NormalNoisePtr &getSoulsandNoise() const { return mSoulsandNoise; }

    const NormalNoisePtr &getNetherStateNoise() const { return mNetherStateNoise; }

    const NormalNoisePtr &getNetherwartNoise() const { return mNetherwartNoise; }

    const DensityFunctionPtr &getBase3dNoise() const { return mBase3dNoise; }

    const DensityFunctionPtr &getDensityFunction() const { return mDensityFunction; }

private:
    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mPatchNoise;
    NormalNoisePtr mSoulsandNoise;
    NormalNoisePtr mNetherStateNoise;
    NormalNoisePtr mNetherwartNoise;
    DensityFunctionPtr mBase3dNoise;
    DensityFunctionPtr mDensityFunction;
};

class NetherDeltaNoises {
public:
    explicit NetherDeltaNoises(IRandom &random);

    const NormalNoisePtr &getSurfaceNoise() const { return mSurfaceNoise; }

    const NormalNoisePtr &getSurfaceSecondaryNoise() const { return mSurfaceSecondaryNoise; }

private:
    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mSurfaceSecondaryNoise;
};

class NetherNoiseHolder {
public:
    explicit NetherNoiseHolder(int64_t seed);

    const NetherTerrainNoises &getTerrainNoises() const { return *mTerrainNoises; }

    const NetherDeltaNoises &getDeltaNoises() const { return *mDeltaNoises; }

    IRandom &getRandomSource() const { return *mRandomSource; }

    static void setInstance(const NetherNoiseHolder *instance);

    static const NetherNoiseHolder *get();

private:
    IRandomPtr mRandomSource;
    std::unique_ptr<NetherTerrainNoises> mTerrainNoises;
    std::unique_ptr<NetherDeltaNoises> mDeltaNoises;
};

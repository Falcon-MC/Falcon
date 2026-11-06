#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>

class NetherTerrainNoises {
public:
    NetherTerrainNoises(IRandom &random, uint32_t seed);

    const NormalNoisePtr &getSurfaceNoise() const { return mSurfaceNoise; }

    const NormalNoisePtr &getPatchNoise() const { return mPatchNoise; }

    const NormalNoisePtr &getSoulsandNoise() const { return mSoulsandNoise; }

    const NormalNoisePtr &getNetherStateNoise() const { return mNetherStateNoise; }

    const NormalNoisePtr &getNetherwartNoise() const { return mNetherwartNoise; }

    const TheEndOctaveNoise &getMinLimitNoise() const { return *mMinLimitNoise; }

    const TheEndOctaveNoise &getMaxLimitNoise() const { return *mMaxLimitNoise; }

    const TheEndOctaveNoise &getMainNoise() const { return *mMainNoise; }

private:
    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mPatchNoise;
    NormalNoisePtr mSoulsandNoise;
    NormalNoisePtr mNetherStateNoise;
    NormalNoisePtr mNetherwartNoise;
    std::unique_ptr<TheEndOctaveNoise> mMinLimitNoise;
    std::unique_ptr<TheEndOctaveNoise> mMaxLimitNoise;
    std::unique_ptr<TheEndOctaveNoise> mMainNoise;
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

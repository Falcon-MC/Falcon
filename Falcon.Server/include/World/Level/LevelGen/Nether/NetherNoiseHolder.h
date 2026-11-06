#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>
#include <vector>

class NetherTerrainNoises {
public:
    explicit NetherTerrainNoises(uint32_t seed);

    const TheEndOctaveNoise &getMinLimitNoise() const { return *mMinLimitNoise; }

    const TheEndOctaveNoise &getMaxLimitNoise() const { return *mMaxLimitNoise; }

    const TheEndOctaveNoise &getMainNoise() const { return *mMainNoise; }

    const std::vector<SurfaceSimplexNoise> &getDepthNoise() const { return mDepthNoise; }

    const std::vector<SurfaceSimplexNoise> &getAdjustmentNoise() const { return mAdjustmentNoise; }

private:
    std::unique_ptr<TheEndOctaveNoise> mMinLimitNoise;
    std::unique_ptr<TheEndOctaveNoise> mMaxLimitNoise;
    std::unique_ptr<TheEndOctaveNoise> mMainNoise;
    std::vector<SurfaceSimplexNoise> mDepthNoise;
    std::vector<SurfaceSimplexNoise> mAdjustmentNoise;
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

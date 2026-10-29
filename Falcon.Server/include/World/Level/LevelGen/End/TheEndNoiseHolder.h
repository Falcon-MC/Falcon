#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"
#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>

class TheEndTerrainNoises {
public:
    explicit TheEndTerrainNoises(int64_t seed);

    const TheEndOctaveNoise &getRoughnessNoiseOctaves() const { return *mRoughnessNoiseOctaves; }

    const TheEndOctaveNoise &getRoughnessNoiseOctaves2() const { return *mRoughnessNoiseOctaves2; }

    const TheEndOctaveNoise &getDetailNoiseOctaves() const { return *mDetailNoiseOctaves; }

    const TheEndSimplexNoise &getIslandNoise() const { return *mIslandNoise; }

private:
    std::unique_ptr<TheEndOctaveNoise> mRoughnessNoiseOctaves;
    std::unique_ptr<TheEndOctaveNoise> mRoughnessNoiseOctaves2;
    std::unique_ptr<TheEndOctaveNoise> mDetailNoiseOctaves;
    std::unique_ptr<TheEndSimplexNoise> mIslandNoise;
};

class TheEndNoiseHolder {
public:
    explicit TheEndNoiseHolder(int64_t seed);

    const TheEndTerrainNoises &getTerrainNoises() const { return *mTerrainNoises; }

    const TheEndSimplexNoise &getDecorationIslandNoise() const { return *mDecorationIslandNoise; }

private:
    std::unique_ptr<TheEndTerrainNoises> mTerrainNoises;
    std::unique_ptr<TheEndSimplexNoise> mDecorationIslandNoise;
};

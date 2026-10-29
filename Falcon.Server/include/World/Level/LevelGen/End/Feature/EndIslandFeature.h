#pragma once

#include "World/Level/LevelGen/End/Object/EndIslandObject.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cstdint>

class TheEndSimplexNoise;

class EndIslandFeature : public IFeature {
public:
    EndIslandFeature(int64_t seed, const TheEndSimplexNoise &islandNoise);

    ~EndIslandFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    int64_t mSeed;
    const TheEndSimplexNoise *mIslandNoise;
    SimpleRandom mChunkRandom;
    EndIslandObject mIslandObject;
};

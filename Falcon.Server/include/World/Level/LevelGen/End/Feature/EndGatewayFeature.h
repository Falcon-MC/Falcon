#pragma once

#include "World/Level/LevelGen/End/Object/EndGatewayObject.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cstdint>

class TheEndSimplexNoise;

class EndGatewayFeature : public IFeature {
public:
    EndGatewayFeature(int64_t seed, const TheEndSimplexNoise &islandNoise);

    ~EndGatewayFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    int64_t mSeed;
    const TheEndSimplexNoise *mIslandNoise;
    SimpleRandom mChunkRandom;
    EndGatewayObject mGatewayObject;
};

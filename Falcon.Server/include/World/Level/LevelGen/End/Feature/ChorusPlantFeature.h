#pragma once

#include "World/Level/LevelGen/End/Tree/LegacyChorusTree.h"
#include "World/Level/LevelGen/Feature/IFeature.h"

#include <cstdint>

class TheEndSimplexNoise;

class ChorusPlantFeature : public IFeature {
public:
    ChorusPlantFeature(int64_t seed, const TheEndSimplexNoise &islandNoise);

    ~ChorusPlantFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    int64_t mSeed;
    const TheEndSimplexNoise *mIslandNoise;
    LegacyChorusTree mChorusTree;
};

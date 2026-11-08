#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

#include <random>

class BlockManager;

class CactusFeature : public CountGenerateFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

    // minecraft:cactus_feature (sub_14D1F3AC0), the village one
    static bool placeCluster(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
};

#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class BlockManager;

class FlowerForestFoliageFeature : public CountGenerateFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

private:
    static void _populateFlower(int32_t flower, BlockManager &object, int32_t x, int32_t y, int32_t z,
                                IRandom &random);
};

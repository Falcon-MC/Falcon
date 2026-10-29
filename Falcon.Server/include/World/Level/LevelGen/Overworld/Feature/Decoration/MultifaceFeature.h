#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class BlockManager;

class MultifaceFeature : public CountGenerateFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

private:
    static int32_t _getOceanFloorHeight(BlockManager &manager, int32_t x, int32_t z, int32_t minY, int32_t maxY);

    static int32_t _placeAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                            int32_t preferredFace);
};

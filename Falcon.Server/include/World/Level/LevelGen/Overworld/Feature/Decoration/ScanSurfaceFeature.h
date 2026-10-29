#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class BlockManager;
class Level;

class ScanSurfaceFeature : public CountGenerateFeature {
public:
    static const int32_t SEA_LEVEL = 63;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

    virtual void place(BlockManager &manager, int32_t x, int32_t y, int32_t z) = 0;

    virtual bool isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const;
};

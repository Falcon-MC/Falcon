#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;

class SpringFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    void _placeSprings(BlockManager &manager, int32_t chunkX, int32_t chunkZ, int32_t count, int32_t minY,
                       int32_t maxY, const BlockState &fluid, bool lava);

    int32_t _nextUniformY(int32_t minY, int32_t maxY);

    int32_t _nextVeryBiasedToBottomY(int32_t minY, int32_t maxY);

    static bool _canPlaceSpring(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};

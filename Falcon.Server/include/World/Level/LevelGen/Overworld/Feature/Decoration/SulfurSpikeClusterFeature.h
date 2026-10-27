#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;
class LevelChunk;

class SulfurSpikeClusterFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    void _tryPlaceClusterSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool hanging,
                               int32_t maxSpikeHeight);

    void _placeSulfurSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool hanging, int32_t maxLength);

    int32_t _nextRangeSafe(int32_t min, int32_t max);
};

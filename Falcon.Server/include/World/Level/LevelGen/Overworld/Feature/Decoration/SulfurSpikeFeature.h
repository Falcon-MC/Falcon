#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;
class LevelChunk;
class SimplexNoise;

class SulfurSpikeFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    void _applyNoiseGradientSurface(LevelChunk &chunk, BlockManager &manager, const SimplexNoise &gradient);

    void _tryPlaceFromFloorScan(BlockManager &manager, const SimplexNoise &gradient, int32_t x, int32_t y, int32_t z);

    void _tryPlaceFromCeilingScan(BlockManager &manager, const SimplexNoise &gradient, int32_t x, int32_t y,
                                  int32_t z);

    void _placeSulfurSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool hanging, int32_t maxLength);

    int32_t _clampedNormal(float deviation, int32_t max);
};

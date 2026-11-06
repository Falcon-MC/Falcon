#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Trial chambers: one candidate per 34 x 34 chunk region, kept unless the biome at chamber height is one of the
 * cave biomes they stay out of.
 */
class TrialChambersFeature : public StructureFeature {
public:
    TrialChambersFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

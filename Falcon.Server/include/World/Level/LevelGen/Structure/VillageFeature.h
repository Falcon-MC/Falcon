#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Villages: one candidate per 34 x 34 chunk region, kept when the surface biome at the centre of the candidate
 * chunk is one a village style exists for.
 */
class VillageFeature : public StructureFeature {
public:
    VillageFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

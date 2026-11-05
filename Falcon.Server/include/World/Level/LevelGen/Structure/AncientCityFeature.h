#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Ancient cities: one candidate per 24 x 24 chunk region, kept when the biome at city height is the deep dark.
 */
class AncientCityFeature : public StructureFeature {
public:
    AncientCityFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

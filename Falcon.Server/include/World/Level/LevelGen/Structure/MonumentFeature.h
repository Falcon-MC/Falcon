#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Ocean monuments: one candidate per 32 x 32 chunk region, kept when deep ocean surrounds the centre and the biomes
 * further out are all water.
 */
class MonumentFeature : public StructureFeature {
public:
    MonumentFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

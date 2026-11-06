#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Desert pyramids, jungle temples, swamp huts and igloos share one grid and one /locate name: one candidate per
 * 32 x 32 chunk region, kept when the surface biome at the centre of the chunk holds one of them.
 */
class TempleFeature : public StructureFeature {
public:
    TempleFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

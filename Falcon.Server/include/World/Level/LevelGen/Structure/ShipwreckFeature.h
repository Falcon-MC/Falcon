#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

/**
 * Shipwrecks: one candidate per 24 x 24 chunk region, kept in the ocean and beach biomes when the biome around the
 * centre does not change and no monument starts within five chunks.
 */
class ShipwreckFeature : public StructureFeature {
public:
    ShipwreckFeature();

    // a start here is a beached wreck, with a shorter pick of pieces and no sinking below the sea
    static bool isBeachBiome(int32_t biomeId);

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

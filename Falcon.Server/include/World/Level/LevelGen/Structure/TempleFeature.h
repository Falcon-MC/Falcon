#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

// desert pyramids, jungle temples, swamp huts and igloos share one grid and one /locate name
class TempleFeature : public StructureFeature {
public:
    TempleFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

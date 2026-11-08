#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class VillageFeature : public StructureFeature {
public:
    VillageFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

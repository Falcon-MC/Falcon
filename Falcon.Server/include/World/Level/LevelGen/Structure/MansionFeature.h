#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class StructureTerrain;

class MansionFeature : public StructureFeature {
public:
    MansionFeature();

    static int32_t getBaseY(StructureTerrain &terrain, int32_t chunkX, int32_t chunkZ, int32_t quarterTurns);

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

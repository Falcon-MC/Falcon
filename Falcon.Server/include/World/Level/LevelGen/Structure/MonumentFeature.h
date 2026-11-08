#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class MonumentFeature : public StructureFeature {
public:
    MonumentFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

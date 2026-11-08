#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class TrialChambersFeature : public StructureFeature {
public:
    TrialChambersFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class RuinedPortalFeature : public StructureFeature {
public:
    RuinedPortalFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

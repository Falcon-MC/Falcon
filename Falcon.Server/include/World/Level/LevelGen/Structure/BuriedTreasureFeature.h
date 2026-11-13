#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

// one start every four chunks on the beaches; the piece is a single chest under the middle of its start chunk
class BuriedTreasureFeature : public StructureFeature {
public:
    BuriedTreasureFeature();

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};

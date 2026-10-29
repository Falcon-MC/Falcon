#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

#include <vector>

class LevelChunk;

class VegetationPatchFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static std::vector<int32_t> _getHighestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z);
};

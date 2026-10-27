#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

#include <vector>

class BlockManager;
class LevelChunk;

class SpeleothemClusterFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static std::vector<int32_t> _getHighestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z);

    static std::vector<int32_t> _getLowestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z);

    void _placePointedDripstone(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool hanging,
                                int32_t maxLength);
};

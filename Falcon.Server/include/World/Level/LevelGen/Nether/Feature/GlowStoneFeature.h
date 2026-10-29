#pragma once

#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

class BlockManager;
class LevelChunk;

class GlowStoneFeature : public NetherFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static int32_t _getHighestWorkableBlock(const LevelChunk &chunk, int32_t x, int32_t z);

    static bool _checkAroundBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};

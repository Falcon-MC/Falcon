#pragma once

#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

class BlockManager;

class NetherSpringFeature : public NetherFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static int32_t _getHighestWorkableBlock(BlockManager &manager, int32_t x, int32_t z);
};

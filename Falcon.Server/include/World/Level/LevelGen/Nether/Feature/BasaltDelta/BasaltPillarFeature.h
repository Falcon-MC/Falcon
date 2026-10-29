#pragma once

#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

#include <vector>

class BlockManager;

class BasaltPillarFeature : public NetherFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static std::vector<int32_t> _getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z);
};

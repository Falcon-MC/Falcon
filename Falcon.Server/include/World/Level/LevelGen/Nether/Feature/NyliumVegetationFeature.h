#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

#include <vector>

class BlockManager;

class NyliumVegetationFeature : public NetherFeature {
public:
    virtual int32_t getBiomeId() const = 0;

    virtual const char *getNyliumIdentifier() const = 0;

    virtual const BlockState &getFungusState() const = 0;

    virtual const BlockState &getRootsState() const = 0;

    void apply(ChunkGenerateContext &context) override;

private:
    std::vector<int32_t> _getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) const;
};

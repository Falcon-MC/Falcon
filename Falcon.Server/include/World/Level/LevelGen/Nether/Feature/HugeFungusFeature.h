#pragma once

#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

#include <memory>
#include <vector>

class BlockManager;
class LegacyNetherTree;

class HugeFungusFeature : public NetherFeature {
public:
    virtual int32_t getBiomeId() const = 0;

    virtual const char *getNyliumIdentifier() const = 0;

    virtual std::shared_ptr<LegacyNetherTree> createTree(int32_t treeHeight) const = 0;

    void apply(ChunkGenerateContext &context) override;

private:
    std::vector<int32_t> _getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) const;
};

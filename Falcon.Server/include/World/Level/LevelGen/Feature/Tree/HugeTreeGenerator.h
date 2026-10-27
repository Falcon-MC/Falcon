#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class HugeTreeGenerator : public VanillaTreeObject {
public:
    HugeTreeGenerator(int32_t baseHeight, int32_t extraRandomHeight, const BlockState &woodMetadata,
                      const BlockState &leavesMetadata);

protected:
    int32_t getHeight(IRandom &random);

    bool ensureGrowable(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t height);

    void growLeavesLayerStrict(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t width);

    void growLeavesLayer(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t width);

    int32_t mBaseHeight;
    int32_t mExtraRandomHeight;
    BlockState mWoodMetadata;
    BlockState mLeavesMetadata;

private:
    bool _isSpaceAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t height);

    bool _ensureDirtsUnderneath(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};

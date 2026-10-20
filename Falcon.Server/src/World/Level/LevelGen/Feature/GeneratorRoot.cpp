#include "World/Level/LevelGen/Feature/GeneratorRoot.h"

#include "World/Level/Block/BlockData.h"

GeneratorRoot::GeneratorRoot(Level &level) : BlockManager(level) {
}

bool GeneratorRoot::canReplace(int32_t x, int32_t y, int32_t z) const {
    const BlockState *cached = getCachedBlock(x, y, z);
    if (cached == nullptr)
        return true;

    if (cached->mName == "minecraft:bamboo")
        return false;

    const BlockData *data = BlockDataTable::find(cached->mName.c_str());
    if (data == nullptr)
        return true;

    return !data->mSolid || data->mTransparent;
}

void GeneratorRoot::setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (!canReplace(x, y, z))
        return;

    BlockManager::setBlockStateAt(x, y, z, state);
}

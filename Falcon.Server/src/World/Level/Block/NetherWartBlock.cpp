#include "World/Level/Block/NetherWartBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

#include "World/Level/Block/Systems/RandomTickSystem.h"

FALCON_REGISTER_BLOCK(NetherWartBlock, 260);

bool NetherWartBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:nether_wart";
}

bool NetherWartBlock::getDrops(const BlockState &state, const ItemStack &, int32_t fortune,
                               std::vector<BlockDrop> &drops) const {
    int count = 1;
    if (state.mStates.getInt(getGrowthState()) == getMaxGrowth())
        count = 2 + RandomTickSystem::nextInt(3) + RandomTickSystem::nextInt(fortune + 1);
    drops.push_back({state.mName, count});
    return true;
}

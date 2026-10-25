#include "World/Level/Block/NetherVinesBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(NetherVinesBlock, 185);

#include <algorithm>
#include <cstdlib>

namespace {
    const float DROP_CHANCE_BY_FORTUNE[] = {0.33f, 0.55f, 0.77f, 1.0f};
    const int32_t MAX_FORTUNE_INDEX = 3;
}

bool NetherVinesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:weeping_vines" || identifier == "minecraft:twisting_vines";
}

bool NetherVinesBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                std::vector<BlockDrop> &drops) const {
    const float chance = DROP_CHANCE_BY_FORTUNE[std::min(std::max(fortuneLevel, 0), MAX_FORTUNE_INDEX)];
    if (hasSilkTouch(tool) || (float) rand() / (float) RAND_MAX < chance)
        drops.push_back({state.mName, 1});
    return true;
}

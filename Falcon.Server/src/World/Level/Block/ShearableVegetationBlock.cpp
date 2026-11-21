#include "World/Level/Block/ShearableVegetationBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ShearableVegetationBlock, 335);

bool ShearableVegetationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:hanging_roots" || identifier == "minecraft:pale_hanging_moss";
}

bool ShearableVegetationBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                        std::vector<BlockDrop> &drops) const {
    (void) fortuneLevel;

    if (isShears(tool) || hasSilkTouch(tool))
        drops.push_back({state.mName, 1});
    return true;
}

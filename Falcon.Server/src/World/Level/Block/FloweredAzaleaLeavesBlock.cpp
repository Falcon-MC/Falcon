#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LeavesBlock.h"

FALCON_REGISTER_BLOCK(FloweredAzaleaLeavesBlock, 295);

bool FloweredAzaleaLeavesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:azalea_leaves_flowered";
}

PistonMoveReaction FloweredAzaleaLeavesBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

bool FloweredAzaleaLeavesBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                         std::vector<BlockDrop> &drops) const {
    LeavesBlock::leafDrops(state, tool, fortuneLevel, drops);
    return true;
}

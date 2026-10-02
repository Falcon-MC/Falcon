#include "Block/Blocks/FloweredAzaleaLeavesBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(FloweredAzaleaLeavesBlock, 295);

bool FloweredAzaleaLeavesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:azalea_leaves_flowered";
}

PistonMoveReaction FloweredAzaleaLeavesBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

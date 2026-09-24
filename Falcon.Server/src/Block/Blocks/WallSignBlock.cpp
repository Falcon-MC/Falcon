#include "Block/Blocks/WallSignBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(WallSignBlock, 185);

bool WallSignBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:wall_sign" || BlockIdentifier::endsWith(identifier, "_wall_sign");
}

PistonMoveReaction WallSignBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

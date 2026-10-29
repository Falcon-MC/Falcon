#include "World/Level/Block/WallSignBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(WallSignBlock, 185);

bool WallSignBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:wall_sign" || BlockIdentifier::endsWith(identifier, "_wall_sign");
}

PistonMoveReaction WallSignBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

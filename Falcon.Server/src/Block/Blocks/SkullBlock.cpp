#include "Block/Blocks/SkullBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(SkullBlock, 205);

bool SkullBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {"minecraft:creeper_head", "minecraft:dragon_head",
                                                   "minecraft:piglin_head", "minecraft:player_head",
                                                   "minecraft:skeleton_skull", "minecraft:wither_skeleton_skull",
                                                   "minecraft:zombie_head"});
}

PistonMoveReaction SkullBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

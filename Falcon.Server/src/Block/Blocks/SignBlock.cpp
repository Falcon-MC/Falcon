#include "Block/Blocks/SignBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(SignBlock, 186);

bool SignBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:standing_sign"
           || BlockIdentifier::endsWithAny(identifier, {"_standing_sign", "_hanging_sign"});
}

PistonMoveReaction SignBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

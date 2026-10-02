#include "Block/Blocks/GlazedTerracottaBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(GlazedTerracottaBlock, 480);

bool GlazedTerracottaBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_glazed_terracotta");
}

PistonMoveReaction GlazedTerracottaBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::PushOnly;
}

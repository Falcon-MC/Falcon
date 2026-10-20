#include "World/Level/Block/GlazedTerracottaBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(GlazedTerracottaBlock, 480);

bool GlazedTerracottaBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_glazed_terracotta");
}

PistonMoveReaction GlazedTerracottaBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::PushOnly;
}

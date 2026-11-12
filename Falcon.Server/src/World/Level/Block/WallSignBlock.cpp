#include "World/Level/Block/WallSignBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/SignEditing.h"

FALCON_REGISTER_BLOCK(WallSignBlock, 185);

bool WallSignBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:wall_sign" || BlockIdentifier::endsWith(identifier, "_wall_sign");
}

PistonMoveReaction WallSignBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

bool WallSignBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                               const BlockState &state) const {
    return SignEditing::interact(owner, player, position, state);
}

void WallSignBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) blockFace;
    SignEditing::onPlaced(owner, player, position, usedItem);
}

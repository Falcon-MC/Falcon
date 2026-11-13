#include "World/Level/Block/SignBlock.h"

#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/OrientationHelpers.h"
#include "World/Level/Block/SignEditing.h"

FALCON_REGISTER_BLOCK(SignBlock, 186);

bool SignBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:standing_sign"
           || BlockIdentifier::endsWithAny(identifier, {"_standing_sign", "_hanging_sign"});
}

PistonMoveReaction SignBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

namespace {
    constexpr const char *STANDING_SUFFIX = "standing_sign";
    constexpr const char *WALL_SUFFIX = "wall_sign";
}

BlockState SignBlock::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    if (context.mFace < FACE_NORTH || !BlockIdentifier::endsWith(state.mName, STANDING_SUFFIX))
        return Block::applyPlacementOrientation(state, context);

    const std::string wall = state.mName.substr(0, state.mName.size() - std::string(STANDING_SUFFIX).size())
                             + WALL_SUFFIX;
    const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(wall);
    if (defaults == nullptr)
        return Block::applyPlacementOrientation(state, context);

    Tag states = *defaults;
    OrientationHelpers::setFacingDirection(states, context.mFace);
    return BlockState(wall, states);
}

bool SignBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) const {
    return SignEditing::interact(owner, player, position, state);
}

void SignBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) blockFace;
    SignEditing::onPlaced(owner, player, position, usedItem);
}

const char *SignBlock::getBlockActorId() const {
    return SignBlockActor::BLOCK_ACTOR_ID;
}

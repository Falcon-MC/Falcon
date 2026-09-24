#include "Block/Blocks/ScaffoldingBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ScaffoldingBlock, 370);

#include "Block/BlockSupport.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

using namespace PlacementHelpers;

namespace {
    const char *STABILITY_CHECK = "stability_check";
    const char *STABILITY = "stability";
    const int UNSTABLE_STABILITY = 7;
}

bool ScaffoldingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:scaffolding";
}

Vector3i ScaffoldingBlock::resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace != PlacementOrientation::FACE_UP)
        return position;

    Vector3i resolved = position;
    while (resolved.y <= level.getMaxY() && stateAt(level, resolved).mName == getIdentifier())
        resolved.y++;

    return resolved;
}

bool ScaffoldingBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    using namespace PlacementOrientation;

    if (LiquidView(stateAt(level, position)).isLava())
        return false;

    const BlockState clicked = stateAt(level, BlockSupport::supportOf(position, blockFace));
    const BlockState below = belowOf(level, position);
    if (clicked.mName == getIdentifier() || below.mName == getIdentifier()
        || DecorationSupport::isAir(below) || DecorationSupport::isSolid(below))
        return true;

    for (int side = FACE_NORTH; side <= FACE_EAST; ++side) {
        if (side == blockFace)
            continue;

        if (stateAt(level, relativePosition(position, side)).mName == getIdentifier())
            return true;
    }

    return false;
}

void ScaffoldingBlock::onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 BlockState &state) const {
    (void) owner;
    (void) level;
    (void) position;

    if (!state.mStates.contains(STABILITY_CHECK))
        return;

    Tag states = state.mStates;
    states.putByte(STABILITY_CHECK, 1);
    state = BlockState(state.mName, states);
}

void ScaffoldingBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) const {
    using namespace PlacementOrientation;

    if (!state.mStates.contains(STABILITY))
        return;

    if (DecorationSupport::isSolid(belowOf(level, position))) {
        if (state.mStates.getInt(STABILITY, 0) == 0 && state.mStates.getByte(STABILITY_CHECK, 0) == 0)
            return;

        Tag states = state.mStates;
        states.putInt(STABILITY, 0);
        states.putByte(STABILITY_CHECK, 0);
        level.setBlock(position, BlockState(state.mName, states), false);
        return;
    }

    int stability = UNSTABLE_STABILITY;
    for (int face = FACE_DOWN; face <= FACE_EAST; ++face) {
        if (face == FACE_UP)
            continue;

        const BlockState side = stateAt(level, relativePosition(position, face));
        if (side.mName != state.mName)
            continue;

        const int sideStability = side.mStates.getInt(STABILITY, UNSTABLE_STABILITY);
        if (sideStability >= stability)
            continue;

        stability = face == FACE_DOWN ? sideStability : sideStability + 1;
    }

    if (stability >= UNSTABLE_STABILITY) {
        BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
        return;
    }

    if (state.mStates.getInt(STABILITY, 0) == stability && state.mStates.getByte(STABILITY_CHECK, 0) == 0)
        return;

    Tag states = state.mStates;
    states.putInt(STABILITY, stability);
    states.putByte(STABILITY_CHECK, 0);
    level.setBlock(position, BlockState(state.mName, states), false);
}

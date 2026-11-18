#include "World/Level/Block/ScaffoldingBlock.h"

#include "World/Actor/Actor.h"
#include "World/Actor/ActorFlags.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ScaffoldingBlock, 370);

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

using namespace PlacementHelpers;

namespace {
    const char *STABILITY_CHECK = "stability_check";
    const char *STABILITY = "stability";
    const int UNSTABLE_STABILITY = 7;
    const float SUPPORT_EPSILON = 0.000001f;
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
    if (clicked.mName == getIdentifier() || below.mName == getIdentifier() || DecorationSupport::isAir(below)
        || DecorationSupport::isSolid(below))
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

bool ScaffoldingBlock::getMoverCollisionShapes(const BlockState &state, const Vector3i &position,
                                               const AxisAlignedBB &mover, bool descending,
                                               std::vector<AxisAlignedBB> &shapes) const {
    const float x = (float) position.x;
    const float y = (float) position.y;
    const float z = (float) position.z;
    if (state.mStates.getInt(STABILITY, 0) == UNSTABLE_STABILITY || descending
        || mover.mMinY < y + 1.0f - SUPPORT_EPSILON)
        return true;
    if (mover.mMaxX > x && mover.mMinX < x + 1.0f && mover.mMaxZ > z && mover.mMinZ < z + 1.0f)
        shapes.push_back(AxisAlignedBB(x, y, z, x + 1.0f, y + 1.0f, z + 1.0f));
    return true;
}

bool ScaffoldingBlock::getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                                           AxisAlignedBB &shape) const {
    // placement, explosions and block rays keep the table shape
    if (actor == nullptr)
        return false;

    const bool carries = state.mStates.getInt(STABILITY, 0) != UNSTABLE_STABILITY
                         && !actor->getFlags().get(ActorFlag::Sneaking)
                         && actor->getPosition().y >= (float) position.y + 1.0f - SUPPORT_EPSILON;
    shape = carries ? AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f) : AxisAlignedBB(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    return true;
}

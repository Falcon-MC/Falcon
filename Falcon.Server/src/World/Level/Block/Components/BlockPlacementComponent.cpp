#include "World/Level/Block/Components/BlockPlacementComponent.h"

#include "World/Level/Block/Block.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/PlacementOrientation.h"

int BlockPlacementComponent::getHorizontalFacing(float yaw) {
    return PlacementOrientation::horizontalFacing(yaw);
}

BlockState BlockPlacementComponent::apply(const BlockState &state, Level *level, float yaw, float pitch,
                                          int face, const Vector3f &clickPosition,
                                          const Vector3f &playerPosition, const Vector3i &blockPosition) {
    using namespace PlacementOrientation;

    BlockPlacementContext context;
    context.mLevel = level;
    context.mYaw = yaw;
    context.mPitch = pitch;
    context.mFace = face;
    context.mClickPosition = clickPosition;
    context.mPlayerPosition = playerPosition;
    context.mBlockPosition = blockPosition;
    context.mPlayerFacing = horizontalFacing(yaw);
    context.mOppositeFacing = context.mPlayerFacing ^ 1;
    context.mOrdinal = horizontalOrdinal(context.mPlayerFacing);
    context.mPistonFacing = pistonFacingDirection(context.mPlayerFacing, playerPosition, blockPosition);

    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (block != nullptr)
        return block->applyPlacementOrientation(state, context);

    const Block fallback(state);
    return fallback.applyPlacementOrientation(state, context);
}

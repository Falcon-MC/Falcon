#include "World/Level/Block/WallAttachedBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/OrientationHelpers.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"

FALCON_REGISTER_BLOCK(WallAttachedBlock, 190);

bool WallAttachedBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ladder"
           || identifier == "minecraft:wall_banner"
           || BlockIdentifier::endsWithAny(identifier, {"_coral_fan", "_coral_wall_fan"})
           || identifier == "minecraft:coral_fan";
}

BlockState WallAttachedBlock::applyPlacementOrientation(const BlockState &state,
                                                        const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : context.mOppositeFacing;
    OrientationHelpers::setFacingDirection(states, facing);
    return BlockState(result.mName, states);
}

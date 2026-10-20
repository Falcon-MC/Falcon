#include "World/Level/Block/FaceAttachedBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/OrientationHelpers.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"

FALCON_REGISTER_BLOCK(FaceAttachedBlock, 210);

bool FaceAttachedBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWithAny(identifier, {"_amethyst_bud", "_cluster"});
}

BlockState FaceAttachedBlock::applyPlacementOrientation(const BlockState &state,
                                                        const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    OrientationHelpers::setFacingDirection(states, context.mFace);
    return BlockState(result.mName, states);
}

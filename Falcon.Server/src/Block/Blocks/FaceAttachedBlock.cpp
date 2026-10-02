#include "Block/Blocks/FaceAttachedBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/OrientationHelpers.h"
#include "Block/Components/BlockPlacementComponent.h"

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

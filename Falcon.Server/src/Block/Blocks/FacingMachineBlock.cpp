#include "Block/Blocks/FacingMachineBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/OrientationHelpers.h"
#include "Block/Components/BlockPlacementComponent.h"

FALCON_REGISTER_BLOCK(FacingMachineBlock, 160);

bool FacingMachineBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {"minecraft:piston", "minecraft:sticky_piston", "minecraft:observer"});
}

BlockState FacingMachineBlock::applyPlacementOrientation(const BlockState &state,
                                                         const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    OrientationHelpers::setFacingDirection(states, context.mPistonFacing);
    return BlockState(result.mName, states);
}

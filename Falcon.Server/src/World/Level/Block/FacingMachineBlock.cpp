#include "World/Level/Block/FacingMachineBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/OrientationHelpers.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"

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

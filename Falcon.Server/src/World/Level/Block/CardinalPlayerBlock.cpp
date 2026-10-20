#include "World/Level/Block/CardinalPlayerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"

FALCON_REGISTER_BLOCK(CardinalPlayerBlock, 230);

bool CardinalPlayerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bed" || BlockIdentifier::endsWith(identifier, "_bed")
           || identifier == "minecraft:fence_gate" || BlockIdentifier::endsWith(identifier, "_fence_gate");
}

BlockState CardinalPlayerBlock::applyPlacementOrientation(const BlockState &state,
                                                          const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    if (states.contains("minecraft:cardinal_direction"))
        states.putString("minecraft:cardinal_direction", cardinalName(context.mPlayerFacing));
    return BlockState(result.mName, states);
}

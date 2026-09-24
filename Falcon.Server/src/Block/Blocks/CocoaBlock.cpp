#include "Block/Blocks/CocoaBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(CocoaBlock, 319);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_COCOA_AGE = 2;
}

bool CocoaBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cocoa";
}

void CocoaBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) owner;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age >= MAX_COCOA_AGE || RandomTickSystem::nextInt(2) != 0)
        return;

    level.setBlock(position, DecorationSupport::withState(state, AGE, age + 1), true);
}

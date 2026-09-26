#include "Block/Blocks/ReedsBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(ReedsBlock, 312);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_REEDS_HEIGHT = 3;
}

bool ReedsBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:reeds";
}

void ReedsBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) owner;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age < MAX_AGE) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, age + 1), false);
        return;
    }

    const Vector3i top = above(position);
    if (!isInRange(level, top) || !isAirAt(level, top))
        return;

    int32_t height = 0;
    Vector3i current = position;
    while (height < MAX_REEDS_HEIGHT && matches(stateAt(level, current).mName)) {
        ++height;
        current = below(current);
    }

    if (height >= MAX_REEDS_HEIGHT)
        return;

    BlockChangeSystem::change(level, top, VanillaBlocks::REEDS().toBlockState(), BlockChangeCause::Grow, true);
    resetAge(level, position, state);
}

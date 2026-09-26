#include "Block/Blocks/KelpBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

FALCON_REGISTER_BLOCK(KelpBlock, 315);

using namespace PlantGrowthHelpers;

namespace {
    const char *KELP_AGE = "kelp_age";

    const int32_t MAX_KELP_AGE = 25;
    const int32_t KELP_GROWTH_PERCENT = 14;
}

bool KelpBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:kelp";
}

void KelpBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    const int32_t age = state.mStates.getInt(KELP_AGE, 0);
    if (age >= MAX_KELP_AGE || RandomTickSystem::nextInt(100) >= KELP_GROWTH_PERCENT)
        return;

    const Vector3i top = above(position);
    if (!isInRange(level, top) || !WaterBlock::isSource(stateAt(level, top)))
        return;

    if (!BlockChangeSystem::change(level, top, DecorationSupport::withState(state, KELP_AGE, age + 1),
                                   BlockChangeCause::Grow, true))
        return;
    level.setBlockStateAtLayer(top.x, top.y, top.z, 1, WaterBlock::source());
    BlockActionHandler::broadcastBlockUpdate(owner, level, top, WaterBlock::source(), 1);
}

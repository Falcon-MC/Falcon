#include "Block/Blocks/SweetBerryBushBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(SweetBerryBushBlock, 318);

using namespace PlantGrowthHelpers;

namespace {
    const char *GROWTH = "growth";
    const int32_t MAX_BERRY_GROWTH = 3;
    const int32_t BERRY_GROWTH_CHANCE = 5;
    const int32_t BERRY_MIN_LIGHT = 9;
}

bool SweetBerryBushBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:sweet_berry_bush";
}

void SweetBerryBushBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;

    const int32_t growth = state.mStates.getInt(GROWTH, 0);
    if (growth >= MAX_BERRY_GROWTH || RandomTickSystem::nextInt(BERRY_GROWTH_CHANCE) != 0)
        return;

    if (RandomTickSystem::getFullLight(level, above(position)) < BERRY_MIN_LIGHT)
        return;

    level.setBlock(position, DecorationSupport::withState(state, GROWTH, growth + 1), true);
}

#include "Block/Blocks/CaveVinesBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(CaveVinesBlock, 317);

using namespace PlantGrowthHelpers;

namespace {
    const char *PLANT_AGE = "growing_plant_age";

    const int32_t MAX_PLANT_AGE = 25;
    const int32_t CAVE_VINES_GROWTH_CHANCE = 10;
    const int32_t CAVE_VINES_BERRY_PERCENT = 11;
}

bool CaveVinesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cave_vines" || identifier == "minecraft:cave_vines_head_with_berries"
           || identifier == "minecraft:cave_vines_body_with_berries";
}

void CaveVinesBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    (void) owner;

    const Vector3i tip = below(position);
    const int32_t age = state.mStates.getInt(PLANT_AGE, 0);
    if (age >= MAX_PLANT_AGE || !isInRange(level, tip) || !isAirAt(level, tip))
        return;

    if (RandomTickSystem::nextInt(CAVE_VINES_GROWTH_CHANCE) != 0)
        return;

    const bool berries = RandomTickSystem::nextInt(100) < CAVE_VINES_BERRY_PERCENT;
    const BlockState grown = berries ? VanillaBlocks::CAVE_VINES_HEAD_WITH_BERRIES().toBlockState()
                                     : VanillaBlocks::CAVE_VINES().toBlockState();
    level.setBlock(tip, DecorationSupport::withState(grown, PLANT_AGE, age + 1), true);

    if (state.mName == "minecraft:cave_vines_head_with_berries") {
        const BlockState body = VanillaBlocks::CAVE_VINES_BODY_WITH_BERRIES().toBlockState();
        level.setBlock(position, DecorationSupport::withState(body, PLANT_AGE, age), false);
    }
}

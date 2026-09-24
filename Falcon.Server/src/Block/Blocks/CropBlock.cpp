#include "Block/Blocks/CropBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CropBlock, 270);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/GrowthHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

#include <string>

using namespace GrowthHelpers;

bool CropBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {
            "minecraft:wheat", "minecraft:carrots", "minecraft:potatoes", "minecraft:beetroot",
            "minecraft:torchflower_crop", "minecraft:pitcher_crop"
    });
}

void CropBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    if (RandomTickSystem::nextInt(getGrowthChance()) != 0)
        return;

    if (needsLight() && RandomTickSystem::getFullLight(level, position) < MINIMUM_LIGHT_LEVEL)
        return;

    const int32_t growth = state.mStates.getInt(getGrowthState());
    if (growth >= getMaxGrowth())
        return;

    level.setBlock(position, withState(state, getGrowthState(), growth + 1), false);
}

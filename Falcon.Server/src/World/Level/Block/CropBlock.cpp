#include "World/Level/Block/CropBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CropBlock, 270);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/GrowthHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"

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

    BlockChangeSystem::change(level, position, withState(state, getGrowthState(), growth + 1), BlockChangeCause::Grow,
                              false);
}

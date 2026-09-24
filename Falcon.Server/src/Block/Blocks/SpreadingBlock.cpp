#include "Block/Blocks/SpreadingBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SpreadingBlock, 300);

#include "Block/BlockLightProperties.h"
#include "Block/Blocks/GrowthHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

using namespace GrowthHelpers;

namespace {
    const int MINIMUM_SPREAD_LIGHT_LEVEL = 4;
    const int MAXIMUM_SPREAD_LIGHT_FILTER = 2;

    int lightFilterAt(Level &level, const Vector3i &position) {
        const BlockState above = level.getBlockState(position.x, position.y, position.z);
        return BlockLightProperties::lightFilter(BlockLightProperties::packed(above));
    }
}

bool SpreadingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:grass_block" || identifier == "minecraft:mycelium";
}

void SpreadingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    const Vector3i above(position.x, position.y + 1, position.z);

    if (lightFilterAt(level, above) > 1) {
        level.setBlock(position, BlockState("minecraft:dirt"), false);
        return;
    }

    if (RandomTickSystem::getFullLight(level, above) < MINIMUM_LIGHT_LEVEL)
        return;

    const Vector3i target(position.x - 1 + RandomTickSystem::nextInt(3),
                          position.y - 3 + RandomTickSystem::nextInt(5),
                          position.z - 1 + RandomTickSystem::nextInt(3));

    if (level.getBlockState(target.x, target.y, target.z).mName != "minecraft:dirt")
        return;

    const Vector3i targetAbove(target.x, target.y + 1, target.z);
    if (RandomTickSystem::getFullLight(level, targetAbove) < MINIMUM_SPREAD_LIGHT_LEVEL
        || lightFilterAt(level, targetAbove) >= MAXIMUM_SPREAD_LIGHT_FILTER)
        return;

    level.setBlock(target, BlockState(state.mName), false);
}

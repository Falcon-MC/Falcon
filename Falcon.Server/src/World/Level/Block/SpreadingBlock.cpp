#include "World/Level/Block/SpreadingBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SpreadingBlock, 300);

#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Block/GrowthHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"

using namespace GrowthHelpers;

namespace {
    const int MINIMUM_SPREAD_LIGHT_LEVEL = 4;
    const int MAXIMUM_SPREAD_LIGHT_FILTER = 2;

    int lightFilterAt(Level &level, const Vector3i &position) {
        const BlockState *above = level.peekBlockPtr(position.x, position.y, position.z);
        return above == nullptr ? 0 : BlockLightProperties::lightFilter(BlockLightProperties::packed(*above));
    }
}

bool SpreadingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:grass_block" || identifier == "minecraft:mycelium";
}

void SpreadingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    const Vector3i above(position.x, position.y + 1, position.z);

    if (lightFilterAt(level, above) > 1) {
        BlockChangeSystem::change(level, position, BlockState("minecraft:dirt"), BlockChangeCause::Fade, false);
        return;
    }

    if (RandomTickSystem::getFullLight(level, above) < MINIMUM_LIGHT_LEVEL)
        return;

    const Vector3i target(position.x - 1 + RandomTickSystem::nextInt(3),
                          position.y - 3 + RandomTickSystem::nextInt(5),
                          position.z - 1 + RandomTickSystem::nextInt(3));

    // Spreading never loads chunks: a target in an unloaded chunk is skipped this tick.
    const BlockState *targetState = level.peekBlockPtr(target.x, target.y, target.z);
    if (targetState == nullptr || targetState->mName != "minecraft:dirt")
        return;

    const Vector3i targetAbove(target.x, target.y + 1, target.z);
    if (RandomTickSystem::getFullLight(level, targetAbove) < MINIMUM_SPREAD_LIGHT_LEVEL
        || lightFilterAt(level, targetAbove) >= MAXIMUM_SPREAD_LIGHT_FILTER)
        return;

    BlockChangeSystem::spread(level, position, target, BlockState(state.mName), false);
}

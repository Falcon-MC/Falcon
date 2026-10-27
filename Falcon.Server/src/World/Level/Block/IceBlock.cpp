#include "World/Level/Block/IceBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(IceBlock, 137);

namespace {
    const int MELT_LIGHT_LEVEL = 12;
}

bool IceBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void IceBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const {
    (void) owner;
    (void) state;

    if (RandomTickSystem::getBlockLight(level, position) < MELT_LIGHT_LEVEL)
        return;

    const BlockState melted = level.getDimensionType() == DimensionType::Nether ? BlockState("minecraft:air")
                                                                               : WaterBlock::source();
    BlockChangeSystem::change(level, position, melted, BlockChangeCause::Fade, true);
}

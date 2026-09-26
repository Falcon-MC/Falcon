#include "Block/Blocks/IceBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

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

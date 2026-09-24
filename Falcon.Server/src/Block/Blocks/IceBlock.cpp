#include "Block/Blocks/IceBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/WaterBlock.h"
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

    if (level.getDimensionType() == DimensionType::Nether)
        level.setBlock(position, BlockState("minecraft:air"), true);
    else
        level.setBlock(position, WaterBlock::source(), true);
}

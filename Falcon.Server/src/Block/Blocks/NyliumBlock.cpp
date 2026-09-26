#include "Block/Blocks/NyliumBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(NyliumBlock, 310);

#include "Block/BlockLightProperties.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Level/Level.h"

namespace {
    bool isTransparentAt(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        return BlockLightProperties::isTransparent(BlockLightProperties::packed(state));
    }
}

bool NyliumBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:crimson_nylium" || identifier == "minecraft:warped_nylium";
}

void NyliumBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) state;

    if (isTransparentAt(level, Vector3i(position.x, position.y + 1, position.z)))
        return;

    BlockChangeSystem::change(level, position, BlockState("minecraft:netherrack"), BlockChangeCause::Fade, false);
}

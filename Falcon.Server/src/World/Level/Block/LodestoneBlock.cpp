#include "World/Level/Block/LodestoneBlock.h"

#include "Network/Handler/LodestoneHandler.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LodestoneBlock, 200);

bool LodestoneBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:lodestone";
}

bool LodestoneBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    (void) state;
    return LodestoneHandler::useCompassOn(owner, player, position);
}

void LodestoneBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) state;
    LodestoneHandler::destroy(owner, level, position);
}

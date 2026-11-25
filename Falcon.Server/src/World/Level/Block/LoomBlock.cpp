#include "World/Level/Block/LoomBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"

FALCON_REGISTER_BLOCK(LoomBlock, 10);

bool LoomBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::LOOM;
}

bool LoomBlock::onInteract(ServerNetworkHandler &, ServerPlayer &player, const Vector3i &position,
                           const BlockState &) const {
    return player.getInventoryManager().onClientOpenLoom(position);
}

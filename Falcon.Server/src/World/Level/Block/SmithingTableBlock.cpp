#include "World/Level/Block/SmithingTableBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SmithingTableBlock, 10);

bool SmithingTableBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:smithing_table";
}

bool SmithingTableBlock::onInteract(ServerNetworkHandler &, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &) const {
    return player.getInventoryManager().onClientOpenSmithing(position);
}

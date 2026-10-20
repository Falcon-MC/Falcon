#include "World/Level/Block/Systems/FurnaceSystem.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"

void FurnaceSystem::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    player.getInventoryManager().tickFurnace(owner);
}

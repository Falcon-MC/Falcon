#include "Block/Systems/FurnaceSystem.h"

#include "Actor/ServerPlayer.h"
#include "Inventory/InventoryManager.h"

void FurnaceSystem::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    player.getInventoryManager().tickFurnace(owner);
}

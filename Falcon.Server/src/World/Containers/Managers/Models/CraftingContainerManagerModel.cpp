#include "World/Containers/Managers/Models/CraftingContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "Network/ServerNetworkHandler.h"

#include <memory>

CraftingContainerManagerModel::CraftingContainerManagerModel()
        : ContainerManagerModel(ContainerType::Workbench, std::make_unique<CraftingContainerScreenValidator>()) {}

bool CraftingContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                               const Vector3i &position) {
    if (!player.getInventoryManager().onClientOpenCraftingTable(position))
        return false;

    player.getInventoryManager().syncCraftingTableState(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    return true;
}

#include "World/Containers/Managers/Models/AnvilContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"

#include <memory>

AnvilContainerManagerModel::AnvilContainerManagerModel()
        : ContainerManagerModel(ContainerType::Anvil, std::make_unique<AnvilContainerScreenValidator>()) {}

bool AnvilContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                            const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().onClientOpenAnvil(position);
}

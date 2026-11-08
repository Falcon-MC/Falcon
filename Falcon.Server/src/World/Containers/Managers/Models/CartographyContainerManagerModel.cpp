#include "World/Containers/Managers/Models/CartographyContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"

#include <memory>

CartographyContainerManagerModel::CartographyContainerManagerModel()
        : ContainerManagerModel(ContainerType::Cartography, std::make_unique<CartographyContainerScreenValidator>()) {
}

bool CartographyContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                                  const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().onClientOpenCartography(position);
}

#include "World/Containers/Managers/Models/StonecutterContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"

#include <memory>

StonecutterContainerManagerModel::StonecutterContainerManagerModel()
        : ContainerManagerModel(ContainerType::Stonecutter, std::make_unique<StonecutterContainerScreenValidator>()) {}

bool StonecutterContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                                  const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().onClientOpenStonecutter(position);
}

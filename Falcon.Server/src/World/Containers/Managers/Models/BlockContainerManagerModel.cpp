#include "World/Containers/Managers/Models/BlockContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"

#include <memory>

BlockContainerManagerModel::BlockContainerManagerModel(ContainerType type)
        : ContainerManagerModel(type, std::make_unique<BlockContainerScreenValidator>()) {}

bool BlockContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                            const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().onClientOpenBlockContainer(position, mType);
}

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "Network/ServerNetworkHandler.h"

#include <utility>

ContainerManagerModel::ContainerManagerModel(ContainerType type,
                                             std::unique_ptr<ContainerScreenValidatorBase> validator)
        : mType(type), mValidator(std::move(validator)) {}

bool ContainerManagerModel::open(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    if (mValidator != nullptr && !mValidator->isValid(owner, player, position))
        return false;

    return openWindow(owner, player, position);
}

bool ContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().openContainer(mType, position);
}

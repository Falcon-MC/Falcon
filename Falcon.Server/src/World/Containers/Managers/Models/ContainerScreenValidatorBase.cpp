#include "World/Containers/Managers/Models/ContainerScreenValidatorBase.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Network/Handler/BlockActionHandler.h"

bool ContainerScreenValidatorBase::isValid(ServerNetworkHandler &owner, ServerPlayer &player,
                                           const Vector3i &position) const {
    (void) owner;

    return isWithinReach(player, position);
}

bool ContainerScreenValidatorBase::isWithinReach(ServerPlayer &player, const Vector3i &position) {
    return BlockActionHandler::canInteractWithBlock(player, position);
}

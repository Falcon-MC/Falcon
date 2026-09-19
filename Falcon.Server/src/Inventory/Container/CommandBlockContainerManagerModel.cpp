#include "Inventory/Container/CommandBlockContainerManagerModel.h"

#include "Actor/ServerPlayer.h"
#include "Block/Actor/CommandBlockActor.h"
#include "Block/Systems/CommandBlockSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <memory>

bool CommandBlockContainerScreenValidator::isValid(ServerNetworkHandler &owner, ServerPlayer &player,
                                                   const Vector3i &position) const {
    if (!player.isOp() && player.getGameType() != (int32_t) GameType::Creative)
        return false;

    return ContainerScreenValidatorBase::isValid(owner, player, position);
}

CommandBlockContainerManagerModel::CommandBlockContainerManagerModel()
        : ContainerManagerModel(ContainerType::CommandBlock,
                                std::make_unique<CommandBlockContainerScreenValidator>()) {}

bool CommandBlockContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                                   const Vector3i &position) {
    CommandBlockActor &actor = CommandBlockSystem::getOrCreate(owner.getLevelFor(player), position);

    BlockActorDataPacket data;
    data.mBlockPosition = actor.getPosition();
    data.mData = actor.getSpawnCompound();
    owner.getNetworkHandler().send(player.getNetworkIdentifier(), data, owner.getCodecContext());

    return ContainerManagerModel::openWindow(owner, player, position);
}

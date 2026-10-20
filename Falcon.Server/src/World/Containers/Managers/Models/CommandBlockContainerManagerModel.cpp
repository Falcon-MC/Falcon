#include "World/Containers/Managers/Models/CommandBlockContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/CommandBlockActor.h"
#include "World/Level/Block/Systems/CommandBlockSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
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

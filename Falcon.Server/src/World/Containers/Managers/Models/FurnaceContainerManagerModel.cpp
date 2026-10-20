#include "World/Containers/Managers/Models/FurnaceContainerManagerModel.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/FurnaceBlock.h"
#include "World/Inventory/InventoryManager.h"

#include <memory>

FurnaceContainerManagerModel::FurnaceContainerManagerModel(const BlockState &state)
        : ContainerManagerModel(FurnaceBlock::containerType(state),
                                std::make_unique<FurnaceContainerScreenValidator>()),
          mState(state) {}

bool FurnaceContainerManagerModel::openWindow(ServerNetworkHandler &owner, ServerPlayer &player,
                                              const Vector3i &position) {
    (void) owner;

    return player.getInventoryManager().onClientOpenFurnace(position, FurnaceBlock::kind(mState), mType);
}

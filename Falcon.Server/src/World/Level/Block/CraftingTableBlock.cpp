#include "World/Level/Block/CraftingTableBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CraftingTableBlock, 10);

#include "World/Containers/Managers/Models/CraftingContainerManagerModel.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

bool CraftingTableBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &state) const {
    (void) state;

    CraftingContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}

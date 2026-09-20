#include "Block/Blocks/CraftingTableBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CraftingTableBlock, 10);

#include "Inventory/Container/CraftingContainerManagerModel.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

bool CraftingTableBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &state) const {
    (void) state;

    CraftingContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}

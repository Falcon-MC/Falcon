#include "Item/Items/HoneycombItem.h"

#include "Actor/ServerPlayer.h"
#include "Block/Systems/CopperSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/ItemClassRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

FALCON_REGISTER_ITEM(HoneycombItem, 101);

HoneycombItem::HoneycombItem(const Item &base) : Item(base) {
}

bool HoneycombItem::matches(const std::string &identifier) {
    return identifier == "minecraft:honeycomb";
}

bool HoneycombItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                 const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    Level &level = owner.getLevelFor(player);
    const BlockState clicked = level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z);
    const std::string waxed = CopperSystem::waxedOf(clicked.mName);
    if (waxed.empty())
        return false;

    CopperSystem::replaceWithPair(owner, level, blockPosition, clicked, waxed);

    LevelEventPacket event;
    event.mEventId = CopperSystem::WAX_ON_EVENT;
    event.mPosition = Vector3f((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                               (float) blockPosition.z + 0.5f);
    event.mData = 0;
    BlockActionHandler::broadcastToViewers(owner, level, event.mPosition, event);

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack remaining = item;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();

        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    return true;
}

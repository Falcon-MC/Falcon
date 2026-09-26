#include "Item/Items/FireChargeItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(FireChargeItem, 41,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:fire_charge";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<FireChargeItem>(item);
                            });

#include "Block/Systems/FireSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/Items/FireStarterHelpers.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

using namespace FireStarterHelpers;

FireChargeItem::FireChargeItem(const Item &base) : Item(base) {}

bool FireChargeItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                  const Vector3i &blockPosition, int32_t face,
                                  const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Adventure)
        return false;

    Level &level = owner.getLevelFor(player);
    const Vector3i placement = relativeToFace(blockPosition, face);
    if (!canIgniteAgainst(level, blockPosition, placement))
        return false;

    if (!FireSystem::ignite(owner, level, placement, isObsidian(level, blockPosition)))
        return false;

    owner.broadcastLevelEvent(level, LevelEventPacket::SoundGhastFireball, centerOf(placement), 0);
    player.consumeOneHeldItem();
    return true;
}

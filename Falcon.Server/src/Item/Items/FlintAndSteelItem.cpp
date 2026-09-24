#include "Item/Items/FlintAndSteelItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(FlintAndSteelItem, 40,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:flint_and_steel";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<FlintAndSteelItem>(item);
                            });

#include "Block/Systems/FireSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/Items/FireStarterHelpers.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

using namespace FireStarterHelpers;

FlintAndSteelItem::FlintAndSteelItem(const Item &base) : Item(base) {}

bool FlintAndSteelItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                     const Vector3i &blockPosition, int32_t face,
                                     const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Adventure)
        return false;

    Level &level = owner.getLevelFor(player);

    const Vector3i placement = relativeToFace(blockPosition, face);
    const bool ignitable = canIgniteAgainst(level, blockPosition, placement);

    if (ignitable)
        FireSystem::ignite(owner, level, placement, isObsidian(level, blockPosition));

    owner.damagePlayerHeldItem(player, 1);
    owner.playLevelSound(level, LevelSoundEvent::FIRE_IGNITE, centerOf(placement));
    return ignitable;
}

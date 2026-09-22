#include "Item/Items/FireStarterItems.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(FlintAndSteelItem, 40,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:flint_and_steel";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<FlintAndSteelItem>(item);
                            });

FALCON_REGISTER_ITEM_CUSTOM(FireChargeItem, 41,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:fire_charge";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<FireChargeItem>(item);
                            });

#include "Block/BlockSupport.h"
#include "Block/Systems/FireSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

namespace {
    Vector3i relativeToFace(const Vector3i &position, int32_t face) {
        switch (face) {
            case 0:
                return Vector3i(position.x, position.y - 1, position.z);
            case 1:
                return Vector3i(position.x, position.y + 1, position.z);
            case 2:
                return Vector3i(position.x, position.y, position.z - 1);
            case 3:
                return Vector3i(position.x, position.y, position.z + 1);
            case 4:
                return Vector3i(position.x - 1, position.y, position.z);
            default:
                return Vector3i(position.x + 1, position.y, position.z);
        }
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isObsidian(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z).mName == "minecraft:obsidian";
    }

    bool canIgniteAgainst(Level &level, const Vector3i &target, const Vector3i &placement) {
        if (level.getBlockState(placement.x, placement.y, placement.z).mName != "minecraft:air")
            return false;

        const BlockState state = level.getBlockState(target.x, target.y, target.z);
        const int burnChance = FireSystem::getBurnChance(state.mName);

        return burnChance != FireSystem::UNBURNABLE && (BlockSupport::isSolid(state) || burnChance > 0);
    }

}

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

    owner.playLevelSound(level, LevelSoundEvent::GHAST_FIREBALL, centerOf(placement));
    player.consumeOneHeldItem();
    return true;
}

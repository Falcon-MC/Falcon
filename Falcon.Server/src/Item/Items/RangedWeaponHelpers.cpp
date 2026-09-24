#include "Item/Items/RangedWeaponHelpers.h"

#include "Actor/ServerPlayer.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>

namespace RangedWeaponHelpers {
    const char *ARROW_IDENTIFIER = "minecraft:arrow";
    const char *ARROW_ACTOR = "minecraft:arrow";

    namespace {
        bool isArrowStack(const ItemStack &stack) {
            return !stack.isAir() && stack.mDefinition != nullptr &&
                   stack.mDefinition->getIdentifier() == ARROW_IDENTIFIER;
        }
    }

    bool hasFiniteResources(const ServerPlayer &player) {
        const int32_t gameType = player.getGameType();
        return gameType == (int32_t) GameType::Survival || gameType == (int32_t) GameType::Adventure;
    }

    int findArrowSlot(const ServerPlayer &player) {
        const PlayerInventory &inventory = player.getInventory();
        if (isArrowStack(inventory.getOffhand()))
            return OFFHAND_SLOT;

        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            if (isArrowStack(inventory.getItem(slot)))
                return slot;
        }
        return -1;
    }

    const ItemStack &arrowAt(const ServerPlayer &player, int slot) {
        const PlayerInventory &inventory = player.getInventory();
        return slot == OFFHAND_SLOT ? inventory.getOffhand() : inventory.getItem(slot);
    }

    void consumeArrow(ServerPlayer &player, int slot) {
        PlayerInventory &inventory = player.getInventory();
        if (slot == OFFHAND_SLOT) {
            ItemStack arrow = inventory.getOffhand();
            arrow.mCount -= 1;
            inventory.setOffhand(arrow.mCount <= 0 ? ItemStack::air() : std::move(arrow));
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
            return;
        }

        ItemStack arrow = inventory.getItem(slot);
        arrow.mCount -= 1;
        if (arrow.mCount <= 0)
            inventory.setItem(slot, ItemStack::air());
        else
            inventory.setItem(slot, std::move(arrow));

        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    }

    float chargeForce(int32_t elapsedTicks, float maxForce) {
        const float p = (float) elapsedTicks / 20.0f;
        return std::min((p * p + p * 2.0f) / 3.0f, 1.0f) * maxForce;
    }
}

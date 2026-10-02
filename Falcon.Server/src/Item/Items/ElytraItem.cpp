#include "Item/Items/ElytraItem.h"

#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemData.h"
#include "Item/ItemDurability.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <utility>

namespace {
    const float GLIDE_FALL_RESET_SPEED = 0.5f;
    const float GLIDE_FALL_RESET_PITCH = 40.0f;
    const int32_t GLIDE_DURABILITY_INTERVAL = 20;
}

bool ElytraItem::isWearingElytra(const ServerPlayer &player) {
    const ItemStack &chestplate = player.getInventory().getArmor(PlayerInventory::ARMOR_TORSO);
    return !chestplate.isAir() && chestplate.mDefinition != nullptr &&
           chestplate.mDefinition->getIdentifier() == IDENTIFIER;
}

void ElytraItem::tickGliding(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.getFlags().get(ActorFlag::Gliding))
        return;

    const auto stopGliding = [&owner, &player]() {
        player.getFlags().set(ActorFlag::Gliding, false);
        owner._sendEntityData(player);
    };

    if (!isWearingElytra(player)) {
        stopGliding();
        return;
    }

    if (std::fabs(player.getMotion().y) < GLIDE_FALL_RESET_SPEED &&
        player.getRotation().x <= GLIDE_FALL_RESET_PITCH)
        player.resetFallDistance();

    if (player.getGameType() == (int32_t) GameType::Creative)
        return;

    PlayerInventory &inventory = player.getInventory();
    ItemStack chestplate = inventory.getArmor(PlayerInventory::ARMOR_TORSO);
    const int32_t unbreaking = ItemEnchantments::getLevel(chestplate, EnchantmentIds::UNBREAKING);
    const int64_t interval = (int64_t) GLIDE_DURABILITY_INTERVAL * (unbreaking + 1);
    if (owner.getCurrentTick() % interval != 0)
        return;

    const ItemData *data = ItemDataTable::find(chestplate.mDefinition->getIdentifier());
    if (data == nullptr || data->mMaxDurability <= 0)
        return;

    if (chestplate.mDamage + 1 >= data->mMaxDurability) {
        stopGliding();
        return;
    }

    if (!ItemDurability::apply(&player, chestplate, 1, false))
        return;
    inventory.setArmor(PlayerInventory::ARMOR_TORSO, std::move(chestplate));
    player.getInventoryManager().syncContents(InventoryManager::InventoryId::Armor);
    InventoryHandler::sendArmorContent(owner, player);
}

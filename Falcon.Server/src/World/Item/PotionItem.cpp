#include "World/Item/PotionItem.h"

#include "World/Item/ItemClassRegistry.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

FALCON_REGISTER_ITEM(PotionItem, 100);

PotionItem::PotionItem(const Item &base) : Item(base) {}

bool PotionItem::matches(const std::string &identifier) {
    return identifier == "minecraft:potion";
}

void PotionItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    owner.applyPotionEffects(player, item.mDamage, 1.0f);
    owner.getLevelFor(player).postGameEvent(GameEvents::DRINK, player.getEyePosition(), &player);
}

#include "Item/Items/PotionItem.h"

#include "Item/ItemClassRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_ITEM(PotionItem, 100);

PotionItem::PotionItem(const Item &base) : Item(base) {
}

bool PotionItem::matches(const std::string &identifier) {
    return identifier == "minecraft:potion";
}

void PotionItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    owner.applyPotionEffects(player, item.mDamage, 1.0f);
}

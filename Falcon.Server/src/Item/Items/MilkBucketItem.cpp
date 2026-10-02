#include "Item/Items/MilkBucketItem.h"

#include "Actor/ServerPlayer.h"
#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(MilkBucketItem, 100);

MilkBucketItem::MilkBucketItem(const Item &base) : Item(base) {
}

bool MilkBucketItem::matches(const std::string &identifier) {
    return identifier == "minecraft:milk_bucket";
}

void MilkBucketItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) owner;
    (void) item;
    player.getEffects().clear();
}

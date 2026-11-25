#include "World/Item/MilkBucketItem.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

FALCON_REGISTER_ITEM(MilkBucketItem, 100);

MilkBucketItem::MilkBucketItem(const Item &base) : Item(base) {}

bool MilkBucketItem::matches(const std::string &identifier) {
    return identifier == "minecraft:milk_bucket";
}

void MilkBucketItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;
    player.getEffects().clear();
    owner.getLevelFor(player).postGameEvent(GameEvents::DRINK, player.getEyePosition(), &player);
}

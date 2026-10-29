#include "World/Item/OminousBottleItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(OminousBottleItem, 100);

#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>

OminousBottleItem::OminousBottleItem(const Item &base) : Item(base) {}

bool OminousBottleItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void OminousBottleItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    const int32_t amplifier = std::max(0, std::min(MAX_AMPLIFIER, item.mDamage));

    // A weaker bottle replaces a stronger Bad Omen instead of being ignored.
    const MobEffectInstance *current = player.getEffect(MobEffectId::BadOmen);
    if (current != nullptr && current->mAmplifier > amplifier)
        player.removeEffect(MobEffectId::BadOmen);

    owner.addEffect(player, MobEffectId::BadOmen, amplifier, BAD_OMEN_DURATION_TICKS);
}

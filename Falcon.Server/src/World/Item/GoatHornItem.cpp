#include "World/Item/GoatHornItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(GoatHornItem, 100);

#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>

namespace {
    const char *const HORN_CALL_SOUND = "horn_call";
    const int32_t MAX_VARIANT = 7;
}

GoatHornItem::GoatHornItem(const Item &base) : Item(base) {}

bool GoatHornItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool GoatHornItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (player.hasItemCooldown(item, owner.getCurrentTick()))
        return true;

    const int32_t variant = std::max(0, std::min(MAX_VARIANT, item.mDamage));
    owner.playLevelSound(owner.getLevelFor(player), HORN_CALL_SOUND + std::to_string(variant), player.getPosition(),
                         "minecraft:player");
    player.startItemCooldown(item, owner.getCurrentTick());
    return true;
}

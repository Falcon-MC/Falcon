#include "World/Item/ItemDurability.h"

#include "Core/Event/EventBus.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ItemProperties.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <random>

namespace {
    std::mt19937 &durabilityRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

bool ItemDurability::apply(ServerPlayer *player, ItemStack &item, int32_t amount, bool rollUnbreaking, EventBus *bus) {
    if (item.isAir() || item.mDefinition == nullptr || amount <= 0)
        return false;

    if (ItemProperties::isUnbreakable(item))
        return false;

    // a pack item keeps its minecraft:durability in the content registry
    const std::string &identifier = item.mDefinition->getIdentifier();
    const ItemData *data = ItemDataTable::find(identifier);
    const CustomItemDefinition *custom = data == nullptr ? CustomContentRegistry::getInstance().findItem(identifier)
                                                         : nullptr;
    const int32_t maxDurability = data != nullptr     ? data->mMaxDurability
                                  : custom != nullptr ? custom->mMaxDurability
                                                      : 0;
    if (maxDurability <= 0)
        return false;

    int32_t applied = rollUnbreaking ? _rollUnbreaking(item, amount) : amount;
    if (applied <= 0)
        return false;

    EventBus *events = bus != nullptr ? bus : player != nullptr ? player->getEventBus() : nullptr;
    if (events != nullptr && events->before().mItemDamage.hasSubscribers()) {
        ItemDamageBeforeEvent damageEvent(player, item, applied);
        events->before().mItemDamage.emit(damageEvent);
        if (damageEvent.isCancelled())
            return false;
        applied = damageEvent.mDamage;
        if (applied <= 0)
            return false;
    }

    item.mDamage += applied;
    if (item.mDamage < maxDurability)
        return true;

    if (events != nullptr && events->after().mItemBreak.hasSubscribers()) {
        ItemBreakAfterEvent breakEvent(player, item);
        events->after().mItemBreak.emit(breakEvent);
    }

    item = ItemStack::air();
    return true;
}

int32_t ItemDurability::_rollUnbreaking(const ItemStack &item, int32_t amount) {
    const int32_t unbreaking = ItemEnchantments::getLevel(item, EnchantmentIds::UNBREAKING);
    if (unbreaking <= 0)
        return amount;

    // each damage point rolled separately, landing with a 1 in (level + 1) chance
    int32_t applied = 0;
    for (int32_t index = 0; index < amount; ++index) {
        if (std::uniform_int_distribution<int32_t>(0, unbreaking)(durabilityRandom()) == 0)
            ++applied;
    }
    return applied;
}

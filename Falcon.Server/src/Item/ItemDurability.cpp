#include "Item/ItemDurability.h"

#include "Actor/ServerPlayer.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"
#include "Plugin/PluginManager.h"

#include <random>

namespace {
    std::mt19937 &durabilityRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

bool ItemDurability::apply(ServerPlayer *player, ItemStack &item, int32_t amount, bool rollUnbreaking) {
    if (item.isAir() || item.mDefinition == nullptr || amount <= 0)
        return false;

    const ItemData *data = ItemDataTable::find(item.mDefinition->getIdentifier());
    if (data == nullptr || data->mMaxDurability <= 0)
        return false;

    int32_t applied = rollUnbreaking ? _rollUnbreaking(item, amount) : amount;
    if (applied <= 0)
        return false;

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ITEM_DAMAGE)) {
        ItemStack damaged = item;
        PluginEvent damageEvent;
        damageEvent.mType = FALCON_EVENT_ITEM_DAMAGE;
        damageEvent.mCancellable = true;
        damageEvent.mPlayer = player;
        damageEvent.mItem = &damaged;
        damageEvent.mAmount = (double) applied;
        plugins->dispatch(damageEvent);
        if (damageEvent.mCancelled)
            return false;
        applied = (int32_t) damageEvent.mAmount;
        if (applied <= 0)
            return false;
    }

    item.mDamage += applied;
    if (item.mDamage < data->mMaxDurability)
        return true;

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ITEM_BREAK)) {
        ItemStack broken = item;
        PluginEvent breakEvent;
        breakEvent.mType = FALCON_EVENT_ITEM_BREAK;
        breakEvent.mPlayer = player;
        breakEvent.mItem = &broken;
        plugins->dispatch(breakEvent);
    }

    item = ItemStack::air();
    return true;
}

int32_t ItemDurability::_rollUnbreaking(const ItemStack &item, int32_t amount) {
    const int32_t unbreaking = ItemEnchantments::getLevel(item, EnchantmentIds::UNBREAKING);
    if (unbreaking <= 0)
        return amount;

    int32_t applied = 0;
    for (int32_t index = 0; index < amount; ++index) {
        if (std::uniform_int_distribution<int32_t>(0, unbreaking)(durabilityRandom()) == 0)
            ++applied;
    }
    return applied;
}

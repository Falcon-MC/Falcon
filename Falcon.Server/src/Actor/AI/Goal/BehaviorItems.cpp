#include "Actor/AI/Goal/BehaviorItems.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>

BehaviorItems::BehaviorItems(const json::Value *items) {
    if (items == nullptr)
        return;

    const auto add = [this](const json::Value &entry) {
        const json::Value *item = entry.isObject() ? entry.get("item") : &entry;
        const json::Value *name = item == nullptr ? entry.get("name") : item;
        if (name != nullptr && name->isString())
            mItems.push_back(LegacyItemMapper::getInstance().resolveWithData(name->mString));
    };

    if (items->isArray()) {
        for (const std::unique_ptr<json::Value> &entry: items->mArray)
            add(*entry);
    } else {
        add(*items);
    }
}

bool BehaviorItems::contains(const ItemStack &item) const {
    if (item.isAir())
        return false;

    return std::find(mItems.begin(), mItems.end(), item.mDefinition->getIdentifier()) != mItems.end();
}

ServerPlayer *BehaviorItems::findNearestHolder(ServerNetworkHandler &owner, const MobActor &mob, float range) const {
    ServerPlayer *nearest = nullptr;
    float nearestDistance = range * range;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != mob.getDimension()
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance > nearestDistance || !contains(player.getInventory().getItemInHand()))
            continue;

        nearest = &player;
        nearestDistance = distance;
    }
    return nearest;
}

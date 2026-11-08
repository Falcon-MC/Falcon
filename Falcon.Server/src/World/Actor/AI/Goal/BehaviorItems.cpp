#include "World/Actor/AI/Goal/BehaviorItems.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Storage/Loot/LegacyItemMapper.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>

BehaviorItems::BehaviorItems(const json::Value *items) {
    if (items == nullptr)
        return;

    const auto add = [this](const json::Value &entry) {
        const json::Value *tags = entry.isObject() ? entry.get("tags") : nullptr;
        if (tags != nullptr && tags->isString()) {
            _addTags(tags->mString);
            return;
        }

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

    if (std::find(mItems.begin(), mItems.end(), item.mDefinition->getIdentifier()) != mItems.end())
        return true;
    if (mTags.empty())
        return false;

    const std::vector<std::string> &tags = CraftingRecipeTable::getItemTags(
            std::string(item.mDefinition->getIdentifier()));
    for (const std::string &tag: mTags) {
        if (std::find(tags.begin(), tags.end(), tag) != tags.end())
            return true;
    }
    return false;
}

void BehaviorItems::_addTags(const std::string &expression) {
    // q.any_tag('a', 'b'): the quoted names; any_tag is the only query item descriptors use here
    size_t start = expression.find('\'');
    while (start != std::string::npos) {
        const size_t end = expression.find('\'', start + 1);
        if (end == std::string::npos)
            return;

        mTags.push_back(expression.substr(start + 1, end - start - 1));
        start = expression.find('\'', end + 1);
    }
}

ServerPlayer *BehaviorItems::findNearestHolder(ServerNetworkHandler &owner, const MobActor &mob, float range) const {
    ServerPlayer *nearest = nullptr;
    float nearestDistance = range * range;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.sharesLevelWith(mob)
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

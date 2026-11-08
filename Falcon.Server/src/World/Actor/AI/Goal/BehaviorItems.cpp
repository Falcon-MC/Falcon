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
    // only a plain q.any_tag('a', 'b'): a negation or all_tags would need Molang, and matching it as any_tag lies
    const std::string::size_type open = expression.find('(');
    const std::string query = expression.substr(0, open);
    if (open == std::string::npos || (query != "q.any_tag" && query != "query.any_tag"))
        return;

    size_t start = expression.find('\'', open);
    while (start != std::string::npos) {
        const size_t end = expression.find('\'', start + 1);
        if (end == std::string::npos)
            return;

        mTags.push_back(expression.substr(start + 1, end - start - 1));
        start = expression.find('\'', end + 1);
    }
}

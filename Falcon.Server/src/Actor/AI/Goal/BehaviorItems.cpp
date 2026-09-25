#include "Actor/AI/Goal/BehaviorItems.h"

#include "Loot/LegacyItemMapper.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>

BehaviorItems::BehaviorItems(const json::Value *items) {
    if (items == nullptr)
        return;

    const auto add = [this](const json::Value &entry) {
        const json::Value *name = entry.isObject() ? entry.get("item") : &entry;
        if (name != nullptr && name->isString())
            mItems.push_back(LegacyItemMapper::getInstance().resolve(name->mString, 0));
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

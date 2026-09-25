#pragma once

#include "Core/Json/Json.h"

#include <string>
#include <vector>

class ItemStack;

class BehaviorItems {
public:
    explicit BehaviorItems(const json::Value *items);

    bool contains(const ItemStack &item) const;

    bool isEmpty() const {
        return mItems.empty();
    }

private:
    std::vector<std::string> mItems;
};

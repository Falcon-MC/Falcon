#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>

enum class ItemLockMode : uint8_t {
    None = 0,
    LockInSlot = 1,
    LockInInventory = 2
};

class ItemProperties {
public:
    static bool canDestroy(const ItemStack &item, const std::string &blockName);
    static bool isUnbreakable(const ItemStack &item);
    static ItemLockMode getLockMode(const ItemStack &item);
    static bool keepsOnDeath(const ItemStack &item);
};

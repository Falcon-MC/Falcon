#include "World/Item/ItemProperties.h"

#include <algorithm>

bool ItemProperties::canDestroy(const ItemStack &item, const std::string &blockName) {
    return std::find(item.mCanBreak.begin(), item.mCanBreak.end(), blockName) != item.mCanBreak.end();
}

bool ItemProperties::isUnbreakable(const ItemStack &item) {
    return item.mTag.isCompound() && item.mTag.getByte("Unbreakable") != 0;
}

ItemLockMode ItemProperties::getLockMode(const ItemStack &item) {
    if (!item.mTag.isCompound())
        return ItemLockMode::None;

    // the game treats any non-zero value other than 1 as locked to the inventory
    const int8_t mode = item.mTag.getByte("minecraft:item_lock");
    if (mode == 0)
        return ItemLockMode::None;
    return mode == 1 ? ItemLockMode::LockInSlot : ItemLockMode::LockInInventory;
}

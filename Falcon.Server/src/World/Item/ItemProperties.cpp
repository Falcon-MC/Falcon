#include "World/Item/ItemProperties.h"

#include <algorithm>

bool ItemProperties::canDestroy(const ItemStack &item, const std::string &blockName) {
    return std::find(item.mCanBreak.begin(), item.mCanBreak.end(), blockName) != item.mCanBreak.end();
}

bool ItemProperties::isUnbreakable(const ItemStack &item) {
    return item.mTag.isCompound() && item.mTag.getByte("Unbreakable") != 0;
}

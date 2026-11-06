#include "World/Item/ItemProperties.h"

#include <algorithm>

bool ItemProperties::canDestroy(const ItemStack &item, const std::string &blockName) {
    return std::find(item.mCanBreak.begin(), item.mCanBreak.end(), blockName) != item.mCanBreak.end();
}

#include "World/Item/ItemProperties.h"

#include "World/Item/ItemData.h"
#include "World/Item/ItemTypes.h"

#include <algorithm>

namespace {
    // netherite is the only armor tier that carries one
    const float NETHERITE_KNOCKBACK_RESISTANCE = 0.1f;
}

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

bool ItemProperties::keepsOnDeath(const ItemStack &item) {
    return item.mTag.isCompound() && item.mTag.getByte("minecraft:keep_on_death") != 0;
}

float ItemProperties::getArmorKnockbackResistance(const std::string &identifier) {
    const ItemData *data = ItemDataTable::find(identifier);
    if (data == nullptr || data->mArmorSlot == ArmorSlot::None)
        return 0.0f;

    return (ArmorTier) data->mArmorTier == ArmorTier::Netherite ? NETHERITE_KNOCKBACK_RESISTANCE : 0.0f;
}

float ItemProperties::getArmorKnockbackResistance(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return 0.0f;

    return getArmorKnockbackResistance(item.mDefinition->getIdentifier());
}

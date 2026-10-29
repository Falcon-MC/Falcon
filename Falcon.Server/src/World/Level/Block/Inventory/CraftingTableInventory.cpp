#include "World/Level/Block/Inventory/CraftingTableInventory.h"

#include "World/Inventory/PlayerInventory.h"

#include <utility>

const ItemStack &CraftingTableInventory::getItem(int slot) const {
    return mOwner.getCraftingTableItem(slot);
}

void CraftingTableInventory::setItem(int slot, ItemStack item) {
    mOwner.setCraftingTableItem(slot, std::move(item));
}

const std::vector<ItemStack> &CraftingTableInventory::getContents() const {
    return mOwner.getCraftingTableContents();
}

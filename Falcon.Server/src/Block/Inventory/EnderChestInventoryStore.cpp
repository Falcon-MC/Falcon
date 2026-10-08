#include "Block/Inventory/EnderChestInventoryStore.h"

EnderChestInventoryStore &EnderChestInventoryStore::getInstance() {
    static EnderChestInventoryStore instance;
    return instance;
}

SimpleContainerInventory &EnderChestInventoryStore::get(int64_t ownerId) {
    auto it = mInventories.find(ownerId);
    if (it != mInventories.end())
        return *it->second;

    std::unique_ptr<SimpleContainerInventory> created(new SimpleContainerInventory(SIZE));
    SimpleContainerInventory &reference = *created;
    mInventories[ownerId] = std::move(created);
    return reference;
}

SimpleContainerInventory *EnderChestInventoryStore::find(int64_t ownerId) {
    auto it = mInventories.find(ownerId);
    return it == mInventories.end() ? nullptr : it->second.get();
}

void EnderChestInventoryStore::remove(int64_t ownerId) {
    mInventories.erase(ownerId);
}

void EnderChestInventoryStore::clear() {
    mInventories.clear();
}

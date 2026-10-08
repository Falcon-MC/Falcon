#pragma once

#include "Block/Inventory/SimpleContainerInventory.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class EnderChestInventoryStore {
public:
    static const int SIZE = 27;

    static EnderChestInventoryStore &getInstance();

    SimpleContainerInventory &get(int64_t ownerId);

    SimpleContainerInventory *find(int64_t ownerId);

    void remove(int64_t ownerId);

    void clear();

private:
    std::unordered_map<int64_t, std::unique_ptr<SimpleContainerInventory>> mInventories;
};

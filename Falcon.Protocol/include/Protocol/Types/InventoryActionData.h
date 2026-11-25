#pragma once

#include "Protocol/Types/InventorySource.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class InventoryActionData {
public:
    InventorySource mSource;
    int32_t mSlot = 0;
    ItemStack mFromItem;
    ItemStack mToItem;
};

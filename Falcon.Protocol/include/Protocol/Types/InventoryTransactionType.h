#pragma once

#include <cstdint>

enum class InventoryTransactionType : int32_t {
    Normal = 0,
    Mismatch = 1,
    ItemUse = 2,
    ItemUseOnEntity = 3,
    ItemRelease = 4,
};

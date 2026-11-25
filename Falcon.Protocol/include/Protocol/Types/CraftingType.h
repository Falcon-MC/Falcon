#pragma once

#include <cstdint>

enum class CraftingType {
    Inventory,
    Crafting,
    Workbench,
};

int32_t craftingTypeToId(CraftingType type);

CraftingType craftingTypeFromId(int32_t id);

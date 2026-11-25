#include "Protocol/Types/CraftingType.h"

int32_t craftingTypeToId(CraftingType type) {
    return (int32_t) type;
}

CraftingType craftingTypeFromId(int32_t id) {
    switch (id) {
        case 0:
            return CraftingType::Inventory;
        case 1:
            return CraftingType::Crafting;
        case 2:
            return CraftingType::Workbench;
        default:
            return CraftingType::Inventory;
    }
}

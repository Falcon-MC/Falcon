#include "Protocol/Types/InventorySource.h"

int32_t inventorySourceTypeToId(InventorySourceType type) {
    switch (type) {
        case InventorySourceType::Invalid:
            return -1;
        case InventorySourceType::Container:
            return 0;
        case InventorySourceType::Global:
            return 1;
        case InventorySourceType::WorldInteraction:
            return 2;
        case InventorySourceType::Creative:
            return 3;
        case InventorySourceType::UntrackedInteractionUi:
            return 100;
        case InventorySourceType::NonImplementedTodo:
            return 99999;
    }
    return -1;
}

InventorySourceType inventorySourceTypeFromId(int32_t id) {
    switch (id) {
        case 0:
            return InventorySourceType::Container;
        case 1:
            return InventorySourceType::Global;
        case 2:
            return InventorySourceType::WorldInteraction;
        case 3:
            return InventorySourceType::Creative;
        case 100:
            return InventorySourceType::UntrackedInteractionUi;
        case 99999:
            return InventorySourceType::NonImplementedTodo;
        default:
            return InventorySourceType::Invalid;
    }
}

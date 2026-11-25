#pragma once

#include <cstdint>

enum class InventorySourceType {
    Invalid,
    Container,
    Global,
    WorldInteraction,
    Creative,
    UntrackedInteractionUi,
    NonImplementedTodo,
};

enum class InventorySourceFlag {
    DropItem,
    PickupItem,
    None,
};

int32_t inventorySourceTypeToId(InventorySourceType type);

InventorySourceType inventorySourceTypeFromId(int32_t id);

class InventorySource {
public:
    InventorySourceType mType = InventorySourceType::Invalid;
    int32_t mContainerId = 0;
    InventorySourceFlag mFlag = InventorySourceFlag::None;
};

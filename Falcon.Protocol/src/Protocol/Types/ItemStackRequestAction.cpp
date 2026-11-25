#include "Protocol/Types/ItemStackRequestAction.h"

#include "Core/Utility/BinaryDataException.h"

#include <string>

namespace {
    const ItemStackRequestActionType TYPES_BY_ID[] = {
        ItemStackRequestActionType::Take,
        ItemStackRequestActionType::Place,
        ItemStackRequestActionType::Swap,
        ItemStackRequestActionType::Drop,
        ItemStackRequestActionType::Destroy,
        ItemStackRequestActionType::Consume,
        ItemStackRequestActionType::Create,
        ItemStackRequestActionType::LabTableCombine,
        ItemStackRequestActionType::BeaconPayment,
        ItemStackRequestActionType::MineBlock,
        ItemStackRequestActionType::CraftRecipe,
        ItemStackRequestActionType::CraftRecipeAuto,
        ItemStackRequestActionType::CraftCreative,
        ItemStackRequestActionType::CraftRecipeOptional,
        ItemStackRequestActionType::CraftRepairAndDisenchant,
        ItemStackRequestActionType::CraftLoom,
        ItemStackRequestActionType::CraftReserved,
        ItemStackRequestActionType::CraftNonImplemented,
        ItemStackRequestActionType::CraftResultsDeprecated,
    };

    const int32_t TYPE_COUNT = sizeof(TYPES_BY_ID) / sizeof(TYPES_BY_ID[0]);
}

int32_t itemStackRequestActionTypeToId(ItemStackRequestActionType type) {
    for (int32_t i = 0; i < TYPE_COUNT; i++) {
        if (TYPES_BY_ID[i] == type) {
            return i;
        }
    }
    return -1;
}

ItemStackRequestActionType itemStackRequestActionTypeFromId(int32_t id) {
    if (id < 0 || id >= TYPE_COUNT) {
        throw BinaryDataException("Unknown ItemStackRequestActionType id " + std::to_string(id));
    }
    return TYPES_BY_ID[id];
}

int32_t itemStackRequestActionIdForVersion(int32_t id, bool craftReserved) {
    const int32_t reserved = itemStackRequestActionTypeToId(ItemStackRequestActionType::CraftReserved);
    return craftReserved || id <= reserved ? id : id - 1;
}

int32_t itemStackRequestActionIdFromVersion(int32_t id, bool craftReserved) {
    const int32_t reserved = itemStackRequestActionTypeToId(ItemStackRequestActionType::CraftReserved);
    return craftReserved || id < reserved ? id : id + 1;
}

int32_t itemStackRequestActionTypeToLegacyId(int32_t id) {
    const int32_t LAB_TABLE_COMBINE = 7;
    if (id >= LAB_TABLE_COMBINE) {
        return id + 2;
    }
    return id;
}

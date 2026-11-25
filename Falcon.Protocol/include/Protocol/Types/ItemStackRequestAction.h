#pragma once

#include "Protocol/Types/ItemDescriptor.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/ItemStackRequestSlotData.h"

#include <cstdint>
#include <string>
#include <vector>

enum class ItemStackRequestActionType {
    Take,
    Place,
    Swap,
    Drop,
    Destroy,
    Consume,
    Create,
    LabTableCombine,
    BeaconPayment,
    MineBlock,
    CraftRecipe,
    CraftRecipeAuto,
    CraftCreative,
    CraftRecipeOptional,
    CraftRepairAndDisenchant,
    CraftLoom,
    CraftReserved,
    CraftNonImplemented,
    CraftResultsDeprecated,
};

int32_t itemStackRequestActionTypeToId(ItemStackRequestActionType type);

ItemStackRequestActionType itemStackRequestActionTypeFromId(int32_t id);

int32_t itemStackRequestActionTypeToLegacyId(int32_t id);

/**
 * Shifts an action ID for versions without CraftReserved, where every later action is one lower.
 */
int32_t itemStackRequestActionIdForVersion(int32_t id, bool craftReserved);

/**
 * The current action ID of an ID received from a version without CraftReserved.
 */
int32_t itemStackRequestActionIdFromVersion(int32_t id, bool craftReserved);

class ItemStackRequestAction {
public:
    ItemStackRequestActionType mType = ItemStackRequestActionType::Take;

    int32_t mCount = 0;
    ItemStackRequestSlotData mSource;
    ItemStackRequestSlotData mDestination;
    bool mRandomly = false;

    int32_t mSlot = 0;

    int32_t mPrimaryEffect = 0;
    int32_t mSecondaryEffect = 0;

    int32_t mHotbarSlot = 0;
    int32_t mPredictedDurability = 0;
    int32_t mStackNetworkId = 0;

    int32_t mRecipeNetworkId = 0;
    int32_t mNumberOfRequestedCrafts = 0;

    int32_t mCreativeItemNetworkId = 0;

    int32_t mFilteredStringIndex = 0;

    int32_t mRepairCost = 0;

    std::string mPatternId;

    std::string mReservedId;
    int32_t mTimesCrafted = 0;

    std::vector<ItemStack> mResultItems;
};

#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <functional>
#include <string>

class MapItemSavedData;

struct CartographyResult {
    ItemStack mResult = ItemStack::air();
    int32_t mInputCost = 0;
    int32_t mAdditionalCost = 0;
};

// the cartography recipes of sub_1436438D0: cloning, extending, locking, upgrading, plus renaming
class CartographyRecipes {
public:
    // the multi recipes of the cartography table, as the client knows them
    static const char *const CLONING_UUID;
    static const char *const EXTENDING_UUID;
    static const char *const LOCKING_UUID;
    static const char *const UPGRADING_UUID;

    struct Maps {
        std::function<MapItemSavedData *(int64_t)> mGetMap;
        // MapDataManager::expandMapByID (sub_141ECFD30)
        std::function<int64_t(int64_t, bool)> mExpand;
        // a locked copy under a fresh id (sub_142F64860)
        std::function<MapItemSavedData *(int64_t)> mLockCopy;
    };

    // name is the text typed in the table, nullptr when the request leaves the name alone
    static CartographyResult createResult(const ItemStack &input, const ItemStack &additional,
                                          const std::string *name, const Maps &maps);
};

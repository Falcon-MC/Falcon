#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <functional>
#include <string>

class MapItemSavedData;

// the locked copy a glass pane makes, created only once the request holds
struct CartographyLock {
    int64_t mOriginalId = -1;
    int64_t mCopyId = -1;
};

// the map a paper extends, recentred only once the craft holds
struct CartographyExpansion {
    int64_t mMapId = -1;
    bool mScaled = false;
};

struct CartographyResult {
    ItemStack mResult = ItemStack::air();
    int32_t mInputCost = 0;
    int32_t mAdditionalCost = 0;
    CartographyLock mLock;
    CartographyExpansion mExpansion;
};

// the cartography recipes: cloning, extending, locking, upgrading, plus renaming
class CartographyRecipes {
public:
    // the multi recipes of the cartography table, as the client knows them
    static const char *const CLONING_UUID;
    static const char *const EXTENDING_UUID;
    static const char *const LOCKING_UUID;
    static const char *const UPGRADING_UUID;

    struct Maps {
        std::function<MapItemSavedData *(int64_t)> mGetMap;
        std::function<int64_t(int64_t, bool)> mExpand;
        // a free id for the locked copy of a map; the copy itself is not made here
        std::function<int64_t(int64_t)> mAllocateLockId;
    };

    // name is the text typed in the table, nullptr when the request leaves the name alone
    static CartographyResult createResult(const ItemStack &input, const ItemStack &additional, const std::string *name,
                                          const Maps &maps);

    // the table slots a MapCreateLockedCopyPacket can stand for: the original map with a glass pane
    static bool isLocking(const ItemStack &input, const ItemStack &additional, int64_t originalId);

    // a filled map one scale out, false when it can't grow; the crafting grid makes it too
    // leaves the maps alone: applyExpansion recentres the parent once the craft holds
    static bool extendMap(const ItemStack &input, const Maps &maps, ItemStack &result, CartographyExpansion &expansion);

    static void applyExpansion(const CartographyExpansion &expansion, const Maps &maps);

    // a map that shows the players, false when it already does; the crafting grid makes it too
    static bool upgradeMap(const ItemStack &input, ItemStack &result);
};

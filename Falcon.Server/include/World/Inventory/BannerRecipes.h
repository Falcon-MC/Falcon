#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <vector>

// the crafting grid's banner recipes the client only knows by uuid: copying a banner and putting one on a shield
class BannerRecipes {
public:
    static const char *const DUPLICATE_UUID;
    static const char *const SHIELD_UUID;

    enum class Kind { None, Duplicate, Shield };

    // what the grid makes and how much of each slot one craft takes
    struct Craft {
        ItemStack mResult = ItemStack::air();
        std::vector<int32_t> mRequiredBySlot;
    };

    // the network ids the crafting data gave the two recipes
    static void setNetworkIds(int32_t duplicate, int32_t shield);

    static Kind kindOf(int32_t networkId);

    // a patterned banner and a blank one of the same base colour, nothing else: two of the patterned one
    static bool duplicate(const std::vector<ItemStack> &grid, Craft &craft);

    // an undecorated shield and a banner of fewer than seven layers: the shield with the banner's layers
    static bool decorateShield(const std::vector<ItemStack> &grid, Craft &craft);

    static bool craft(Kind kind, const std::vector<ItemStack> &grid, Craft &craft);
};

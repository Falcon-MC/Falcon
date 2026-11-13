#pragma once

#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Item/CraftingRecipeTable.h"

class StonecutterRecipes {
public:
    static bool matchesInput(const StonecutterRecipeData &recipe, const ItemStack &input);

    // one craft's output; air when the output item is unknown
    static ItemStack createOutput(const StonecutterRecipeData &recipe, const PacketCodecContext &context);
};

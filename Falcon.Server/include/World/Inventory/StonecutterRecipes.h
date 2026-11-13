#pragma once

#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Item/CraftingRecipeTable.h"

#include <vector>

class StonecutterRecipes {
public:
    static bool matchesInput(const StonecutterRecipeData &recipe, const ItemStack &input);

    // one craft's output; air when the output item is unknown
    // the game walks its stonecutter recipes by id: the client indexes this order
    static std::vector<const StonecutterRecipeData *> validRecipes(const ItemStack &input);

    static ItemStack createOutput(const StonecutterRecipeData &recipe, const PacketCodecContext &context);
};

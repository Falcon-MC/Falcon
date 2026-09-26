#pragma once

#include "Item/CraftingRecipeTable.h"
#include "Inventory/CraftingGrid.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <vector>

struct CraftingRecipeMatch {
    const CraftingRecipeData *mRecipe = nullptr;
    std::vector<int32_t> mRequiredBySlot;
};

class CraftingManager {
public:
    static bool matchRecipe(const std::vector<ItemStack> &grid, int gridWidth,
                            const CraftingRecipeData &recipe, CraftingRecipeMatch &match);

    static bool matchNetworkRecipe(const std::vector<ItemStack> &grid, int gridWidth, int32_t networkId,
                                   const std::vector<ItemStack> &recipeOutputs,
                                   const std::vector<uint32_t> &recipeSourceIndices, CraftingRecipeMatch &match);

    static ItemStack findResult(const std::vector<ItemStack> &grid, int gridWidth,
                                const std::vector<ItemStack> &recipeOutputs,
                                const std::vector<uint32_t> &recipeSourceIndices);

    static bool matchResult(const std::vector<ItemStack> &grid, int gridWidth, const ItemStack &result,
                            const std::vector<ItemStack> &recipeOutputs,
                            const std::vector<uint32_t> &recipeSourceIndices, CraftingRecipeMatch &match,
                            ItemStack &outOutput);
};

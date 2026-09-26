#include "Inventory/CraftingManager.h"

#include "Scripting/Content/CustomContentRegistry.h"

#include <functional>
#include <string>
#include <utility>

namespace {
    bool isAir(const ItemStack &item) {
        return item.isAir() || item.mCount <= 0;
    }

    bool matchesIngredient(const ItemStack &item, const CraftingIngredientData &ingredient) {
        if (ingredient.mItemId == nullptr || isAir(item) || item.mDefinition == nullptr) {
            return ingredient.mItemId == nullptr && isAir(item);
        }

        if (std::string(item.mDefinition->getIdentifier()) != ingredient.mItemId) {
            return false;
        }

        return (ingredient.mAuxValue < 0 || item.mDamage == ingredient.mAuxValue)
               && item.mCount >= ingredient.mCount;
    }

    bool matchShaped(const std::vector<ItemStack> &grid, int gridWidth, const CraftingRecipeData &recipe,
                    const CraftingIngredientData *ingredients, CraftingRecipeMatch &match) {
        if (recipe.mWidth <= 0 || recipe.mHeight <= 0 || recipe.mWidth > gridWidth || recipe.mHeight > gridWidth
            || recipe.mIngredientCount < (uint32_t) (recipe.mWidth * recipe.mHeight)) {
            return false;
        }

        for (int offsetY = 0; offsetY <= gridWidth - recipe.mHeight; ++offsetY) {
            for (int offsetX = 0; offsetX <= gridWidth - recipe.mWidth; ++offsetX) {
                for (int mirrored = 0; mirrored < 2; ++mirrored) {
                    std::vector<int32_t> required(grid.size(), 0);
                    bool valid = true;

                    for (int y = 0; y < gridWidth && valid; ++y) {
                        for (int x = 0; x < gridWidth; ++x) {
                            const bool inside = x >= offsetX && x < offsetX + recipe.mWidth
                                                && y >= offsetY && y < offsetY + recipe.mHeight;
                            const int gridSlot = y * gridWidth + x;
                            if (!inside) {
                                if (!isAir(grid[(size_t) gridSlot])) {
                                    valid = false;
                                }
                                continue;
                            }

                            const int recipeX = x - offsetX;
                            const int recipeY = y - offsetY;
                            const int sourceX = mirrored ? recipe.mWidth - recipeX - 1 : recipeX;
                            const CraftingIngredientData &ingredient =
                                    ingredients[recipe.mIngredientOffset + (size_t) recipeY * recipe.mWidth + sourceX];
                            if (!matchesIngredient(grid[(size_t) gridSlot], ingredient)) {
                                valid = false;
                                continue;
                            }
                            if (ingredient.mItemId != nullptr) {
                                required[(size_t) gridSlot] = ingredient.mCount;
                            }
                        }
                    }

                    if (valid) {
                        match.mRecipe = &recipe;
                        match.mRequiredBySlot = std::move(required);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool matchShapeless(const std::vector<ItemStack> &grid, int gridWidth, const CraftingRecipeData &recipe,
                       const CraftingIngredientData *ingredients, CraftingRecipeMatch &match) {
        if (recipe.mWidth != 0 || recipe.mHeight != 0 || recipe.mIngredientCount == 0
            || grid.size() != (size_t) gridWidth * gridWidth) {
            return false;
        }

        std::vector<int> occupied;
        for (size_t slot = 0; slot < grid.size(); ++slot) {
            if (!isAir(grid[slot])) {
                occupied.push_back((int) slot);
            }
        }
        if (occupied.size() != recipe.mIngredientCount) {
            return false;
        }

        std::vector<bool> used(occupied.size(), false);
        std::vector<int32_t> required(grid.size(), 0);
        std::function<bool(size_t)> assign = [&](size_t ingredientIndex) {
            if (ingredientIndex == recipe.mIngredientCount) {
                return true;
            }

            const CraftingIngredientData &ingredient = ingredients[recipe.mIngredientOffset + ingredientIndex];
            for (size_t occupiedIndex = 0; occupiedIndex < occupied.size(); ++occupiedIndex) {
                if (used[occupiedIndex]) {
                    continue;
                }

                const int gridSlot = occupied[occupiedIndex];
                if (!matchesIngredient(grid[(size_t) gridSlot], ingredient)) {
                    continue;
                }

                used[occupiedIndex] = true;
                required[(size_t) gridSlot] = ingredient.mCount;
                if (assign(ingredientIndex + 1)) {
                    return true;
                }
                required[(size_t) gridSlot] = 0;
                used[occupiedIndex] = false;
            }
            return false;
        };

        if (!assign(0)) {
            return false;
        }

        match.mRecipe = &recipe;
        match.mRequiredBySlot = std::move(required);
        return true;
    }

    bool matchesCustomIngredient(const ItemStack &item, const CustomRecipeIngredient &ingredient) {
        if (ingredient.mEmpty || ingredient.mItemId.empty()) {
            return isAir(item);
        }
        if (isAir(item) || item.mDefinition == nullptr) {
            return false;
        }
        return std::string(item.mDefinition->getIdentifier()) == ingredient.mItemId
               && item.mCount >= ingredient.mCount;
    }

    bool matchCustomShaped(const std::vector<ItemStack> &grid, int gridWidth, const CustomRecipe &recipe,
                           CraftingRecipeMatch &match) {
        if (recipe.mWidth <= 0 || recipe.mHeight <= 0 || recipe.mWidth > gridWidth || recipe.mHeight > gridWidth
            || recipe.mInputs.size() != (size_t) (recipe.mWidth * recipe.mHeight)) {
            return false;
        }

        for (int offsetY = 0; offsetY <= gridWidth - recipe.mHeight; ++offsetY) {
            for (int offsetX = 0; offsetX <= gridWidth - recipe.mWidth; ++offsetX) {
                std::vector<int32_t> required(grid.size(), 0);
                bool valid = true;

                for (int y = 0; y < gridWidth && valid; ++y) {
                    for (int x = 0; x < gridWidth; ++x) {
                        const bool inside = x >= offsetX && x < offsetX + recipe.mWidth
                                            && y >= offsetY && y < offsetY + recipe.mHeight;
                        const int gridSlot = y * gridWidth + x;
                        if (!inside) {
                            if (!isAir(grid[(size_t) gridSlot])) {
                                valid = false;
                            }
                            continue;
                        }

                        const int recipeX = x - offsetX;
                        const int recipeY = y - offsetY;
                        const CustomRecipeIngredient &ingredient =
                                recipe.mInputs[(size_t) recipeY * recipe.mWidth + recipeX];
                        if (!matchesCustomIngredient(grid[(size_t) gridSlot], ingredient)) {
                            valid = false;
                            continue;
                        }
                        if (!ingredient.mEmpty) {
                            required[(size_t) gridSlot] = ingredient.mCount;
                        }
                    }
                }

                if (valid) {
                    match.mRequiredBySlot = std::move(required);
                    return true;
                }
            }
        }
        return false;
    }

    bool matchCustomShapeless(const std::vector<ItemStack> &grid, int gridWidth, const CustomRecipe &recipe,
                              CraftingRecipeMatch &match) {
        std::vector<const CustomRecipeIngredient *> ingredients;
        for (const CustomRecipeIngredient &ingredient: recipe.mInputs) {
            if (!ingredient.mEmpty)
                ingredients.push_back(&ingredient);
        }

        std::vector<int> occupied;
        for (size_t slot = 0; slot < grid.size(); ++slot) {
            if (!isAir(grid[slot])) {
                occupied.push_back((int) slot);
            }
        }
        if (occupied.empty() || occupied.size() != ingredients.size()) {
            return false;
        }

        std::vector<bool> used(occupied.size(), false);
        std::vector<int32_t> required(grid.size(), 0);
        std::function<bool(size_t)> assign = [&](size_t ingredientIndex) {
            if (ingredientIndex == ingredients.size()) {
                return true;
            }
            for (size_t k = 0; k < occupied.size(); ++k) {
                if (used[k]) {
                    continue;
                }
                if (matchesCustomIngredient(grid[(size_t) occupied[k]], *ingredients[ingredientIndex])) {
                    used[k] = true;
                    required[(size_t) occupied[k]] = ingredients[ingredientIndex]->mCount;
                    if (assign(ingredientIndex + 1)) {
                        return true;
                    }
                    used[k] = false;
                    required[(size_t) occupied[k]] = 0;
                }
            }
            return false;
        };

        if (!assign(0)) {
            return false;
        }

        match.mRequiredBySlot = std::move(required);
        return true;
    }

    bool matchCustomRecipe(const std::vector<ItemStack> &grid, int gridWidth, const CustomRecipe &recipe,
                           CraftingRecipeMatch &match) {
        if (!CraftingGrid::isValidWidth(gridWidth) || grid.size() != (size_t) CraftingGrid::getSize(gridWidth)) {
            return false;
        }
        return recipe.mShaped ? matchCustomShaped(grid, gridWidth, recipe, match)
                              : matchCustomShapeless(grid, gridWidth, recipe, match);
    }
}

bool CraftingManager::matchRecipe(const std::vector<ItemStack> &grid, int gridWidth,
                                  const CraftingRecipeData &recipe, CraftingRecipeMatch &match) {
    if (!CraftingGrid::isValidWidth(gridWidth) || grid.size() != (size_t) CraftingGrid::getSize(gridWidth)) {
        return false;
    }

    const CraftingIngredientData *ingredients = CraftingRecipeTable::getIngredients();
    if (ingredients == nullptr) {
        return false;
    }

    return recipe.mWidth > 0
           ? matchShaped(grid, gridWidth, recipe, ingredients, match)
           : matchShapeless(grid, gridWidth, recipe, ingredients, match);
}

bool CraftingManager::matchNetworkRecipe(const std::vector<ItemStack> &grid, int gridWidth, int32_t networkId,
                                         const std::vector<ItemStack> &recipeOutputs,
                                         const std::vector<uint32_t> &recipeSourceIndices,
                                         CraftingRecipeMatch &match) {
    if (networkId <= 0 || (size_t) networkId > recipeOutputs.size()
        || recipeOutputs[(size_t) networkId - 1].isAir()
        || (size_t) networkId > recipeSourceIndices.size()) {
        return false;
    }

    const size_t sourceIndex = recipeSourceIndices[(size_t) networkId - 1];
    if (sourceIndex >= CraftingRecipeTable::getRecipeCount()) {
        const size_t customIndex = sourceIndex - CraftingRecipeTable::getRecipeCount();
        const std::vector<CustomRecipe> &customRecipes = CustomContentRegistry::getInstance().getRecipes();
        if (customIndex >= customRecipes.size()) {
            return false;
        }
        return matchCustomRecipe(grid, gridWidth, customRecipes[customIndex], match);
    }

    return matchRecipe(grid, gridWidth, CraftingRecipeTable::getRecipes()[sourceIndex], match);
}

ItemStack CraftingManager::findResult(const std::vector<ItemStack> &grid, int gridWidth,
                                      const std::vector<ItemStack> &recipeOutputs,
                                      const std::vector<uint32_t> &recipeSourceIndices) {
    CraftingRecipeMatch match;
    for (size_t index = 0; index < recipeOutputs.size() && index < recipeSourceIndices.size(); ++index) {
        if (matchNetworkRecipe(grid, gridWidth, (int32_t) index + 1, recipeOutputs, recipeSourceIndices, match)) {
            return recipeOutputs[index];
        }
    }
    return ItemStack::air();
}

bool CraftingManager::matchResult(const std::vector<ItemStack> &grid, int gridWidth, const ItemStack &result,
                                  const std::vector<ItemStack> &recipeOutputs,
                                  const std::vector<uint32_t> &recipeSourceIndices, CraftingRecipeMatch &match,
                                  ItemStack &outOutput) {
    if (result.isAir() || result.mDefinition == nullptr) {
        return false;
    }

    for (size_t index = 0; index < recipeOutputs.size() && index < recipeSourceIndices.size(); ++index) {
        const ItemStack &candidate = recipeOutputs[index];
        if (candidate.isAir() || candidate.mDefinition == nullptr
            || std::string(candidate.mDefinition->getIdentifier()) != result.mDefinition->getIdentifier()
            || candidate.mDamage != result.mDamage || candidate.mCount != result.mCount) {
            continue;
        }

        if (matchNetworkRecipe(grid, gridWidth, (int32_t) index + 1, recipeOutputs, recipeSourceIndices, match)) {
            outOutput = candidate;
            return true;
        }
    }
    return false;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct CraftingIngredientData {
    const char *mItemId;
    int32_t mAuxValue;
    int32_t mCount;
};

struct CraftingOutputData {
    const char *mItemId;
    int32_t mCount;
};

struct CraftingRecipeData {
    const char *mRecipeId;
    int32_t mWidth;
    int32_t mHeight;
    uint32_t mIngredientOffset;
    uint32_t mIngredientCount;
    uint32_t mOutputOffset;
    uint32_t mOutputCount;
    const char *mUuid;
    int32_t mPriority;
};

struct FurnaceRecipeData {
    const char *mRecipeId;
    const char *mInputItemId;
    int32_t mInputAuxValue;
    int32_t mInputCount;
    const char *mOutputItemId;
    int32_t mOutputCount;
    int32_t mPriority;
};

class CraftingRecipeTable {
public:
    static const CraftingRecipeData *getRecipes();
    static size_t getRecipeCount();
    static const CraftingIngredientData *getIngredients();
    static const CraftingOutputData *getOutputs();

    static const FurnaceRecipeData *getFurnaceRecipes();
    static size_t getFurnaceRecipeCount();

    static const std::vector<std::string> &getItemTags(const std::string &identifier);
};

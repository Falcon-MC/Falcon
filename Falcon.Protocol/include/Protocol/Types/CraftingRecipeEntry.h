#pragma once

#include "Core/NBT/Tag.h"
#include "Core/Utility/UUID.h"

#include <cstdint>
#include <string>
#include <vector>

enum class RecipeIngredientType : int {
    Name = 0,
    Molang = 1,
    ItemTag = 2
};

struct RecipeIngredientEntry {
    bool mHasItem = false;
    RecipeIngredientType mType = RecipeIngredientType::Name;
    std::string mItemId;
    int32_t mAuxValue = -1;
    std::string mItemTag;
    std::string mMolangExpression;
    int16_t mMolangVersion = 0;
    int32_t mCount = 1;
};

struct RecipeOutputEntry {
    int32_t mRuntimeId = 0;
    int32_t mCount = 1;
    int32_t mMeta = 0;
    int32_t mBlockRuntimeId = 0;
    bool mIsShield = false;
    Tag mTag;
};

struct CraftingRecipeEntry {
    std::string mRecipeId;
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::vector<RecipeIngredientEntry> mInputs;
    std::vector<RecipeOutputEntry> mOutputs;
    Uuid mUuid;
    std::string mBlockName;
    int32_t mPriority = 0;
    bool mSymmetric = false;
    bool mHasUnlockingRequirement = true;
    int32_t mUnlockingContext = 1;
    bool mHasUnlockingIngredients = false;
    std::vector<RecipeIngredientEntry> mUnlockingIngredients;
    int32_t mRecipeNetId = 0;
};

struct MultiRecipeEntry {
    Uuid mUuid;
    int32_t mRecipeNetId = 0;
};

struct SmithingRecipeEntry {
    std::string mRecipeId;
    RecipeIngredientEntry mTemplate;
    RecipeIngredientEntry mInput;
    RecipeIngredientEntry mAddition;
    RecipeOutputEntry mOutput;
    std::string mBlockName;
    int32_t mRecipeNetId = 0;
};

struct PotionMixEntry {
    int32_t mInputId = 0;
    int32_t mInputMeta = 0;
    int32_t mReagentId = 0;
    int32_t mReagentMeta = 0;
    int32_t mOutputId = 0;
    int32_t mOutputMeta = 0;
};

struct PotionContainerMixEntry {
    int32_t mInputId = 0;
    int32_t mReagentId = 0;
    int32_t mOutputId = 0;
};

struct MaterialReducerOutputEntry {
    int32_t mItemId = 0;
    int32_t mCount = 0;
};

struct MaterialReducerEntry {
    int32_t mInputId = 0;
    int32_t mInputMeta = 0;
    std::vector<MaterialReducerOutputEntry> mOutputs;
};

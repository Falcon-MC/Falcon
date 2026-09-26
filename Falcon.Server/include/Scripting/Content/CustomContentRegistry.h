#pragma once

#include "Actor/ActorPropertySchema.h"
#include "Core/NBT/Tag.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

class BehaviorPackManager;
class ItemDefinitionRegistry;
class BlockDefinitionRegistry;

struct CustomItemDefinition {
    std::string mIdentifier;
    std::string mDisplayName;
    std::string mIcon;
    std::string mCreativeCategory = "items";
    std::string mCreativeGroup;
    int32_t mMaxStackSize = 64;
    bool mIsFood = false;
    int32_t mCooldownTicks = 0;
    std::string mCooldownCategory;
    int32_t mNutrition = 0;
    float mSaturationModifier = 0.6f;
    bool mCanAlwaysEat = false;
    int32_t mMaxDurability = 0;
    bool mHandEquipped = false;
    std::vector<std::string> mCustomComponents;
    bool mHasDigger = false;
    bool mDiggerUseEfficiency = false;
    int32_t mDiggerToolSpeed = 0;
    std::unordered_map<std::string, int32_t> mDiggerBlockSpeeds;
    int32_t mNetworkId = 0;
};

struct CustomBlockBox {
    bool mEnabled = true;
    float mOrigin[3] = {-8.0f, 0.0f, -8.0f};
    float mSize[3] = {16.0f, 16.0f, 16.0f};
};

struct CustomBlockDefinition {
    std::string mIdentifier;
    std::string mDisplayName;
    std::string mTexture;
    std::string mRenderMethod = "opaque";
    std::string mMenuCategory = "construction";
    std::string mCreativeGroup;
    int32_t mNetworkHash = 0;
    int32_t mVanillaBlockId = 0;
    int32_t mItemNetworkId = 0;
    float mDestroyTime = 0.5f;
    float mExplosionResistance = 1.0f;
    int32_t mLightEmission = 0;
    float mFriction = 0.6f;
    bool mHasDestroyTime = false;
    bool mHasExplosionResistance = false;
    bool mHasFriction = false;
    CustomBlockBox mCollision;
    CustomBlockBox mSelection;
};

struct CustomActorDefinition {
    std::string mIdentifier;
    std::string mRuntimeIdentifier;
    bool mIsSummonable = true;
    bool mIsSpawnable = false;
    bool mIsExperimental = false;
    bool mIsProjectile = false;
    float mCollisionWidth = 0.6f;
    float mCollisionHeight = 1.8f;
    std::vector<std::string> mTypeFamilies;
    std::vector<ActorPropertyDescription> mProperties;

    const ActorPropertyDescription *findProperty(const std::string &name) const;
};

struct CustomRecipeIngredient {
    std::string mItemId;
    int32_t mCount = 1;
    bool mEmpty = false;
};

struct CustomRecipe {
    std::string mIdentifier;
    bool mShaped = false;
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::vector<CustomRecipeIngredient> mInputs;
    std::string mResultItem;
    int32_t mResultCount = 1;
};

class CustomContentRegistry {
public:
    void load(const BehaviorPackManager &packs, ItemDefinitionRegistry &items, BlockDefinitionRegistry &blocks);

    const std::vector<BlockPropertyData> &getBlockProperties() const { return mBlockProperties; }

    const std::vector<CustomItemDefinition> &getItems() const { return mItems; }

    const std::vector<CustomBlockDefinition> &getBlocks() const { return mBlocks; }

    const std::vector<CustomActorDefinition> &getActors() const { return mActors; }

    const std::vector<CustomRecipe> &getRecipes() const { return mRecipes; }

    const CustomActorDefinition *getActorDefinition(const std::string &identifier) const;

    int32_t getItemMaxStackSize(const std::string &identifier) const;

    int32_t getDiggerSpeed(const std::string &itemId, const std::string &blockName, bool blockNeedsStandardTool,
                           bool &useEfficiency) const;

    bool isCustomBlock(const std::string &identifier) const;

    bool hasIdentifier(const std::string &identifier) const;

    const CustomItemDefinition *registerItem(const CustomItemDefinition &item, ItemDefinitionRegistry &items);

    const CustomBlockDefinition *registerBlock(const CustomBlockDefinition &block, ItemDefinitionRegistry &items,
                                               BlockDefinitionRegistry &blocks);

    const CustomActorDefinition *registerActor(const CustomActorDefinition &actor);

    void freeze() {
        mFrozen = true;
    }

    bool isFrozen() const {
        return mFrozen;
    }

    static CustomContentRegistry &getInstance();

private:
    void _loadPackItems(const std::string &packPath, ItemDefinitionRegistry &items);

    void _loadPackBlocks(const std::string &packPath, ItemDefinitionRegistry &items, BlockDefinitionRegistry &blocks);

    void _loadPackActors(const std::string &packPath);

    void _loadPackRecipes(const std::string &packPath);

    void _registerItem(const CustomItemDefinition &item, ItemDefinitionRegistry &items);

    void _registerBlock(const CustomBlockDefinition &block, ItemDefinitionRegistry &items,
                        BlockDefinitionRegistry &blocks);

    Tag _buildItemComponentData(const CustomItemDefinition &item, ItemDefinitionRegistry &items);

    Tag _buildBlockPropertyNbt(const CustomBlockDefinition &block);

    int32_t mNextItemNetworkId = 20000;
    int32_t mNextBlockRuntimeId = 10000;
    std::vector<CustomItemDefinition> mItems;
    std::vector<CustomBlockDefinition> mBlocks;
    std::vector<CustomActorDefinition> mActors;
    std::vector<CustomRecipe> mRecipes;
    std::vector<BlockPropertyData> mBlockProperties;
    bool mFrozen = false;
};

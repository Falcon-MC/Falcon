#pragma once

#include "World/Actor/Projectile/ProjectileComponent.h"
#include "World/Level/Block/BlockPlacementFilter.h"
#include "World/Actor/ActorPropertySchema.h"
#include "Core/NBT/Tag.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Item/ItemTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

class BehaviorPackManager;
class ItemDefinitionRegistry;
class BlockDefinitionRegistry;

// one minecraft:repairable repair_items entry: repairing items and repair_amount (Molang or number)
struct CustomRepairEntry {
    std::vector<std::string> mItems;
    std::string mAmount;
};

// a custom component bound in the JSON; params is its JSON text, empty in the 1.x list form
struct CustomComponentUse {
    std::string mName;
    std::string mParams;
};

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
    // minecraft:enchantable: slot lowercased as the game looks it up
    bool mHasEnchantable = false;
    std::string mEnchantableSlot = "none";
    int32_t mEnchantableValue = 0;
    // minecraft:damage: attack damage on top of the empty hand's
    int32_t mDamage = 0;
    std::vector<std::string> mTags;
    // the defaults of an item that declares none of these
    bool mAllowOffHand = false;
    bool mCanDestroyInCreative = true;
    bool mGlint = false;
    bool mLiquidClipped = false;
    bool mShouldDespawn = true;
    bool mStackedByData = false;
    bool mFireResistant = false;
    // minecraft:hover_text_color: empty when the item keeps the default name color
    std::string mHoverTextColor;
    // minecraft:dyeable: default_color as 0xRRGGBB, white when the JSON leaves it out
    bool mHasDyeable = false;
    uint32_t mDyeableDefaultColor = 0xFFFFFF;
    // minecraft:interact_button: true or false keeps the game's default text
    bool mHasInteractButton = false;
    std::string mInteractButtonText = "action.interact.use";
    // minecraft:block_placer: the block the item places, and the blocks it may be used on (any when empty)
    bool mHasBlockPlacer = false;
    std::string mPlacerBlock;
    std::vector<std::string> mPlacerUseOn;
    bool mPlacerReplacesBlockItem = false;
    bool mPlacerAligned = false;
    // minecraft:fuel: seconds of burning
    bool mHasFuel = false;
    float mFuelDuration = 0.0f;
    // minecraft:compostable: chance out of 100
    bool mHasCompostable = false;
    int32_t mCompostingChance = 0;
    // minecraft:use_modifiers: use_duration in seconds
    bool mHasUseModifiers = false;
    float mUseDuration = 0.0f;
    float mMovementModifier = 1.0f;
    bool mEmitVibrations = true;
    std::string mUseStartSound;
    std::string mStartUsing = "always";
    // minecraft:use_animation: empty when the item declares none
    std::string mUseAnimation;
    // minecraft:wearable: the slot name as declared, and its armor slot when it is one of the four
    bool mHasWearable = false;
    std::string mWearableSlotName;
    ArmorSlot mWearableSlot = ArmorSlot::None;
    int32_t mProtection = 0;
    bool mHidesPlayerLocation = false;
    std::vector<CustomComponentUse> mCustomComponents;
    bool mHasDigger = false;
    bool mDiggerUseEfficiency = false;
    int32_t mDiggerToolSpeed = 0;
    std::unordered_map<std::string, int32_t> mDiggerBlockSpeeds;
    std::vector<CustomRepairEntry> mRepairItems;
    int32_t mNetworkId = 0;
};

// a description.states entry: its values in declaration order, the first is the default
struct CustomBlockStateValues {
    std::string mName;
    std::vector<Tag> mValues;
};

// a permutations[] entry: its Molang condition and the custom components it adds
struct CustomBlockPermutation {
    std::string mCondition;
    std::vector<CustomComponentUse> mCustomComponents;
    // its components as declared, overlaid on the block's when sent to the client
    std::string mComponents;
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
    std::string mGeometry = "minecraft:geometry.full_block";
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
    BlockPlacementFilter mPlacementFilter;
    std::vector<CustomComponentUse> mCustomComponents;
    std::vector<CustomBlockStateValues> mStates;
    std::vector<CustomBlockPermutation> mPermutations;
    bool mHasTick = false;
    bool mTickLooping = true;
    int32_t mTickMin = 10;
    int32_t mTickMax = 10;
    bool mRedstoneConsumer = false;
    int32_t mRedstoneMinPower = 0;
    bool mEntityFallOn = false;
    float mMinFallDistance = 0.0f;
    // minecraft:instrument_sound: up and down faces to a note sound
    bool mInstrumentSound = false;
    std::vector<std::pair<std::string, std::string>> mInstrumentSounds;
    // minecraft:neighbor_change: one bit a face (down 0 ... east 5)
    uint8_t mNeighborDirections = 0;
    // minecraft:vibration_properties, both optional and false by default
    bool mCanDampenVibrations = false;
    bool mCanOccludeVibrations = false;
    // minecraft:block_entity entity_storage: a data-driven block actor keeps entities for scripts
    bool mEntityStorage = false;
};

struct CustomActorDefinition {
    std::string mIdentifier;
    std::string mRuntimeIdentifier;
    bool mIsSummonable = true;
    bool mIsSpawnable = false;
    bool mIsExperimental = false;
    bool mIsProjectile = false;
    bool mHasProjectileComponent = false;
    ProjectileComponent mProjectileComponent;
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

    const std::vector<BlockPropertyData> &getBlockProperties() const {
        return mBlockProperties;
    }

    const std::vector<CustomItemDefinition> &getItems() const {
        return mItems;
    }

    const std::vector<CustomBlockDefinition> &getBlocks() const {
        return mBlocks;
    }

    const std::vector<CustomActorDefinition> &getActors() const {
        return mActors;
    }

    const std::vector<CustomRecipe> &getRecipes() const {
        return mRecipes;
    }

    const CustomActorDefinition *getActorDefinition(const std::string &identifier) const;

    int32_t getItemMaxStackSize(const std::string &identifier) const;

    const CustomItemDefinition *findItem(const std::string &identifier) const;

    int32_t getDiggerSpeed(const std::string &itemId, const std::string &blockName, bool blockNeedsStandardTool,
                           bool &useEfficiency) const;

    bool isCustomBlock(const std::string &identifier) const;

    const CustomBlockDefinition *findBlock(const std::string &identifier) const;

    // every combination of the block's states; one empty compound for a block without states
    static std::vector<Tag> permutationsOf(const CustomBlockDefinition &block);

    static Tag defaultStatesOf(const CustomBlockDefinition &block);

    // the block's components, then those of each permutation whose condition holds; a later one replaces
    static std::vector<CustomComponentUse> componentsFor(const CustomBlockDefinition &block, const Tag &states);

    bool hasIdentifier(const std::string &identifier) const;

    // the faces a block's minecraft:neighbor_change watches, 0 for any other block
    uint8_t neighborDirectionsOf(const std::string &identifier) const {
        const auto found = mNeighborListeners.find(identifier);
        return found == mNeighborListeners.end() ? 0 : found->second;
    }

    bool hasNeighborListeners() const {
        return !mNeighborListeners.empty();
    }

    // the world's upcoming_creator_features experiment, read before the packs load
    void setUpcomingCreatorFeatures(bool enabled) {
        mUpcomingCreatorFeatures = enabled;
    }

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

    // a host running world after world loads the next world's packs from scratch
    void reset();

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
    std::unordered_map<std::string, uint8_t> mNeighborListeners;
    bool mUpcomingCreatorFeatures = false;
    bool mFrozen = false;
};

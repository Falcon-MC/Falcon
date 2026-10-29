#pragma once

#include "World/Item/Components/NetworkedItemComponent.h"
#include "World/Item/ItemTypes.h"

#include <string>
#include <vector>

class AllowOffHandItemComponent final : public NetworkedItemComponent<AllowOffHandItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:allow_off_hand"; }
    bool mAllowed = true;
    bool allowsOffHand() const { return mAllowed; }
};

class ArmorItemComponent final : public NetworkedItemComponent<ArmorItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:armor"; }
    ArmorSlot mSlot = ArmorSlot::None;
    int32_t mTier = 0;
    int32_t mPoints = 0;
    int32_t mToughness = 0;
    ArmorSlot getSlot() const { return mSlot; }
    int32_t getPoints() const { return mPoints; }
    int32_t getToughness() const { return mToughness; }
};

class BundleInteractionItemComponent final : public NetworkedItemComponent<BundleInteractionItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:bundle_interaction"; }
    bool mEnabled = true;
    bool isEnabled() const { return mEnabled; }
};

class CanDestroyInCreativeItemComponent final : public NetworkedItemComponent<CanDestroyInCreativeItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:can_destroy_in_creative"; }
    bool mEnabled = true;
    bool canDestroyInCreative() const { return mEnabled; }
};

class CompostableItemComponent final : public NetworkedItemComponent<CompostableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:compostable"; }
    float mChance = 0.0f;
    float getChance() const { return mChance; }
};

class CooldownItemComponent final : public NetworkedItemComponent<CooldownItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:cooldown"; }
    std::string mCategory;
    int32_t mDurationTicks = 0;
    const std::string &getCategory() const { return mCategory; }
    int32_t getDurationTicks() const { return mDurationTicks; }
};

class DamageItemComponent final : public NetworkedItemComponent<DamageItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:damage"; }
    int32_t mDamage = 0;
    int32_t getDamage() const { return mDamage; }
};

class DiggerItemComponent final : public NetworkedItemComponent<DiggerItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:digger"; }
    ToolType mToolType = ToolType::None;
    int32_t mTier = 0;
    int32_t mHarvestLevel = 0;
    ToolType getToolType() const { return mToolType; }
    int32_t getTier() const { return mTier; }
    int32_t getHarvestLevel() const { return mHarvestLevel; }
};

class DisplayNameItemComponent final : public NetworkedItemComponent<DisplayNameItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:display_name"; }
    std::string mValue;
    const std::string &getText() const { return mValue; }
};

class DurabilityItemComponent final : public NetworkedItemComponent<DurabilityItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:durability"; }
    int32_t mMaxDurability = 0;
    int32_t getMaxDurability() const { return mMaxDurability; }
    bool isBroken(int32_t damage) const { return mMaxDurability > 0 && damage >= mMaxDurability; }
};

class DyeableItemComponent final : public NetworkedItemComponent<DyeableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:dyeable"; }
    bool mEnabled = true;
    bool isDyeable() const { return mEnabled; }
};

class EnchantableItemComponent final : public NetworkedItemComponent<EnchantableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:enchantable"; }
    int32_t mValue = 0;
    int32_t getValue() const { return mValue; }
};

class EntityPlacerItemComponent final : public NetworkedItemComponent<EntityPlacerItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:entity_placer"; }
    std::string mEntityType;
    const std::string &getEntityType() const { return mEntityType; }
};

class FireResistantItemComponent final : public NetworkedItemComponent<FireResistantItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:fire_resistant"; }
    bool mEnabled = true;
    bool isFireResistant() const { return mEnabled; }
};

struct FoodEffect {
    int32_t mEffectId = 0;
    int32_t mAmplifier = 0;
    int32_t mDurationTicks = 0;
    float mChance = 1.0f;
};

class FoodItemComponent final : public NetworkedItemComponent<FoodItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:food"; }
    int32_t mNutrition = 0;
    float mSaturation = 0.0f;
    bool mCanAlwaysEat = false;
    bool mFastFood = false;
    std::vector<FoodEffect> mEffects;

    int32_t getNutrition() const { return mNutrition; }
    float getSaturation() const { return mSaturation; }
    float getSaturationModifier() const { return mSaturation; }
    const std::vector<FoodEffect> &getEffects() const { return mEffects; }
    bool canAlwaysEat() const { return mCanAlwaysEat; }
    bool isFastFood() const { return mFastFood; }
    bool isEdible() const { return mNutrition > 0; }
    void addEffect(const FoodEffect &effect) { mEffects.push_back(effect); }
};

class FuelItemComponent final : public NetworkedItemComponent<FuelItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:fuel"; }
    int32_t mBurnTimeTicks = 0;
    int32_t getBurnTimeTicks() const { return mBurnTimeTicks; }
};

class GlintItemComponent final : public NetworkedItemComponent<GlintItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:glint"; }
    bool mEnabled = true;
    bool hasGlint() const { return mEnabled; }
};

class HandEquippedItemComponent final : public NetworkedItemComponent<HandEquippedItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:hand_equipped"; }
    bool mEnabled = true;
    bool isHandEquipped() const { return mEnabled; }
};

class HoverTextColorItemComponent final : public NetworkedItemComponent<HoverTextColorItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:hover_text_color"; }
    std::string mColor;
    const std::string &getColor() const { return mColor; }
};

class IconItemComponent final : public NetworkedItemComponent<IconItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:icon"; }
    std::string mTexture;
    const std::string &getTexture() const { return mTexture; }
};

class KineticWeaponItemComponent final : public NetworkedItemComponent<KineticWeaponItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:kinetic_weapon"; }
    int32_t mDamage = 0;
    int32_t getDamage() const { return mDamage; }
};

class LiquidClippedItemComponent final : public NetworkedItemComponent<LiquidClippedItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:liquid_clipped"; }
    bool mEnabled = true;
    bool isLiquidClipped() const { return mEnabled; }
};

class MaxStackSizeItemComponent final : public NetworkedItemComponent<MaxStackSizeItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:max_stack_size"; }
    int32_t mValue = 64;
    int32_t getValue() const { return mValue; }
};

class PiercingWeaponItemComponent final : public NetworkedItemComponent<PiercingWeaponItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:piercing_weapon"; }
    int32_t mLevel = 0;
    int32_t getLevel() const { return mLevel; }
};

class PlanterItemComponent final : public NetworkedItemComponent<PlanterItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:planter"; }
    std::vector<std::string> mBlocks;
    const std::vector<std::string> &getBlocks() const { return mBlocks; }
};

class ProjectileItemComponent final : public NetworkedItemComponent<ProjectileItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:projectile"; }
    std::string mProjectileType;
    const std::string &getProjectileType() const { return mProjectileType; }
};

class RarityItemComponent final : public NetworkedItemComponent<RarityItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:rarity"; }
    int32_t mValue = 0;
    int32_t getValue() const { return mValue; }
};

class RecordItemComponent final : public NetworkedItemComponent<RecordItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:record"; }
    std::string mSoundEvent;
    const std::string &getSoundEvent() const { return mSoundEvent; }
};

class RenderOffsetsItemComponent final : public NetworkedItemComponent<RenderOffsetsItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:render_offsets"; }
    float mScale = 1.0f;
    float getScale() const { return mScale; }
};

class RepairableItemComponent final : public NetworkedItemComponent<RepairableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:repairable"; }
    std::vector<std::string> mItems;
    const std::vector<std::string> &getItems() const { return mItems; }
};

class ShooterItemComponent final : public NetworkedItemComponent<ShooterItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:shooter"; }
    std::string mProjectileType;
    const std::string &getProjectileType() const { return mProjectileType; }
};

class ShouldDespawnItemComponent final : public NetworkedItemComponent<ShouldDespawnItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:should_despawn"; }
    bool mEnabled = true;
    bool shouldDespawn() const { return mEnabled; }
};

class StackedByDataItemComponent final : public NetworkedItemComponent<StackedByDataItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:stacked_by_data"; }
    bool mEnabled = true;
    bool isStackedByData() const { return mEnabled; }
};

class StorageItemComponent final : public NetworkedItemComponent<StorageItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:storage"; }
    int32_t mSlots = 0;
    int32_t getSlots() const { return mSlots; }
};

class StorageWeightLimitItemComponent final : public NetworkedItemComponent<StorageWeightLimitItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:storage_weight_limit"; }
    int32_t mLimit = 0;
    int32_t getLimit() const { return mLimit; }
};

class StorageWeightModifierItemComponent final : public NetworkedItemComponent<StorageWeightModifierItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:storage_weight_modifier"; }
    float mModifier = 0.0f;
    float getModifier() const { return mModifier; }
};

class SwingDurationItemComponent final : public NetworkedItemComponent<SwingDurationItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:swing_duration"; }
    int32_t mDurationTicks = 0;
    int32_t getDurationTicks() const { return mDurationTicks; }
};

class SwingSoundsItemComponent final : public NetworkedItemComponent<SwingSoundsItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:swing_sounds"; }
    std::string mMainHand;
    std::string mOffHand;
    const std::string &getMainHandSound() const { return mMainHand; }
    const std::string &getOffHandSound() const { return mOffHand; }
};

class TagsItemComponent final : public NetworkedItemComponent<TagsItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:tags"; }
    std::vector<std::string> mTags;
    const std::vector<std::string> &getTags() const { return mTags; }
};

class ThrowableItemComponent final : public NetworkedItemComponent<ThrowableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:throwable"; }
    bool mEnabled = true;
    bool isThrowable() const { return mEnabled; }
};

class UseAnimationItemComponent final : public NetworkedItemComponent<UseAnimationItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:use_animation"; }
    int32_t mAnimation = 0;
    int32_t getAnimation() const { return mAnimation; }
};

class UseModifiersItemComponent final : public NetworkedItemComponent<UseModifiersItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:use_modifiers"; }
    float mUseDuration = 0.0f;
    float mMovementModifier = 1.0f;
    float getDuration() const { return mUseDuration; }
    float getMovementModifier() const { return mMovementModifier; }
};

class WearableItemComponent final : public NetworkedItemComponent<WearableItemComponent> {
public:
    static constexpr const char *componentName() { return "minecraft:wearable"; }
    ArmorSlot mSlot = ArmorSlot::None;
    ArmorSlot getSlot() const { return mSlot; }
};

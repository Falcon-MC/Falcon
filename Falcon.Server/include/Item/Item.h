#pragma once

#include "Block/Block.h"
#include "Item/Components/ItemComponents.h"
#include "Item/ItemData.h"
#include "Item/ItemCooldowns.h"
#include "Item/ItemTypes.h"

#include <cstdint>
#include <string>
#include <utility>

class ServerNetworkHandler;
class ServerPlayer;
class ItemStack;
class Actor;
struct Vector3f;
struct Vector3i;

class Item {
public:
    virtual ~Item() = default;

    virtual float getAttackDamageBonus(const ItemStack &item, ServerPlayer &attacker) const {
        (void) item;
        (void) attacker;
        return 0.0f;
    }

    virtual float getArmorEfficiency(const ItemStack &item) const {
        (void) item;
        return 1.0f;
    }

    virtual void onPostAttack(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &victim, float damage,
                              const ItemStack &item) const {
        (void) owner;
        (void) attacker;
        (void) victim;
        (void) damage;
        (void) item;
    }

    virtual bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
        (void) owner;
        (void) player;
        (void) item;
        return false;
    }

    virtual bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                              const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
        (void) owner;
        (void) player;
        (void) item;
        (void) blockPosition;
        (void) face;
        (void) clickPosition;
        return false;
    }

    virtual bool takesPriorityOverBlockInteraction() const {
        return false;
    }

    virtual bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
        (void) owner;
        (void) player;
        (void) item;
        return false;
    }

    virtual void onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                             int32_t elapsedTicks) const {
        (void) owner;
        (void) player;
        (void) item;
        (void) elapsedTicks;
    }

    virtual bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                             int32_t elapsedTicks) const {
        (void) owner;
        (void) player;
        (void) item;
        (void) elapsedTicks;
        return false;
    }

    virtual bool canAlwaysEat() const {
        return false;
    }

    virtual void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
        (void) owner;
        (void) player;
        (void) item;
    }

    Item() : mTypeId(0), mIdentifier("minecraft:air"), mName("Air"), mMaxStackSize(64), mIsBlock(false) {
        initializeComponents();
    }

    Item(int32_t typeId, const std::string &identifier, const std::string &name, int32_t maxStackSize = 64)
            : mTypeId(typeId), mIdentifier(identifier), mName(name), mMaxStackSize(maxStackSize), mIsBlock(false) {
        initializeComponents();
    }

    Item(const Block &block, int32_t maxStackSize = 64)
            : mTypeId(block.getTypeId()), mIdentifier(block.getIdentifier()), mName(block.getName()),
              mMaxStackSize(maxStackSize), mIsBlock(true), mBlock(block) {
        initializeComponents();
    }

    explicit Item(const ItemData &data)
            : mTypeId(data.mTypeId), mIdentifier(data.mIdentifier), mName(data.mName),
              mMaxStackSize(data.mMaxStackSize), mIsBlock(false), mMaxDurability(data.mMaxDurability),
              mToolType(data.mToolType), mToolTier(data.mToolTier), mHarvestLevel(data.mHarvestLevel),
              mAttackDamage(data.mAttackDamage), mArmorSlot(data.mArmorSlot), mArmorTier(data.mArmorTier),
              mArmorPoints(data.mArmorPoints), mArmorToughness(data.mArmorToughness),
              mNutrition(data.mNutrition), mSaturation(data.mSaturation) {
        initializeComponents();
    }

    int32_t getTypeId() const { return mTypeId; }

    const std::string &getIdentifier() const { return mIdentifier; }

    const std::string &getName() const {
        const auto *component = mComponents.get<DisplayNameItemComponent>();
        return component == nullptr ? mName : component->getText();
    }

    int32_t getMaxStackSize() const {
        const auto *component = mComponents.get<MaxStackSizeItemComponent>();
        return component == nullptr ? mMaxStackSize : component->getValue();
    }

    bool isBlock() const { return mIsBlock; }

    const Block &getBlock() const { return mBlock; }

    int32_t getMaxDurability() const {
        const auto *component = mComponents.get<DurabilityItemComponent>();
        return component == nullptr ? mMaxDurability : component->getMaxDurability();
    }

    bool isDurable() const { return mMaxDurability > 0; }

    ToolType getToolType() const { return mToolType; }

    bool isTool() const { return mToolType != ToolType::None; }

    int32_t getToolTier() const { return mToolTier; }

    int32_t getHarvestLevel() const { return mHarvestLevel; }

    int32_t getAttackDamage() const {
        const auto *component = mComponents.get<DamageItemComponent>();
        return component == nullptr ? mAttackDamage : component->getDamage();
    }

    ArmorSlot getArmorSlot() const { return mArmorSlot; }

    bool isArmor() const { return mArmorSlot != ArmorSlot::None; }

    int32_t getArmorTier() const { return mArmorTier; }

    int32_t getArmorPoints() const { return mArmorPoints; }

    int32_t getArmorToughness() const { return mArmorToughness; }

    int32_t getNutrition() const {
        const auto *component = mComponents.get<FoodItemComponent>();
        return component == nullptr ? mNutrition : component->getNutrition();
    }

    float getSaturation() const {
        const auto *component = mComponents.get<FoodItemComponent>();
        return component == nullptr ? mSaturation : component->getSaturation();
    }

    bool isEdible() const {
        const auto *component = mComponents.get<FoodItemComponent>();
        return component != nullptr && component->isEdible();
    }

    ItemComponents &getComponents() { return mComponents; }

    const ItemComponents &getComponents() const { return mComponents; }

    template <typename T>
    T *getComponent() { return mComponents.get<T>(); }

    template <typename T>
    const T *getComponent() const { return mComponents.get<T>(); }

    template <typename T, typename... Args>
    T &addComponent(Args &&... args) {
        return mComponents.emplace<T>(std::forward<Args>(args)...);
    }

private:
    void initializeComponents() {
        auto &displayName = mComponents.emplace<DisplayNameItemComponent>();
        displayName.mValue = mName;

        auto &maxStackSize = mComponents.emplace<MaxStackSizeItemComponent>();
        maxStackSize.mValue = mMaxStackSize;

        auto &damage = mComponents.emplace<DamageItemComponent>();
        damage.mDamage = mAttackDamage;

        if (mMaxDurability > 0) {
            auto &durability = mComponents.emplace<DurabilityItemComponent>();
            durability.mMaxDurability = mMaxDurability;
        }

        if (mToolType != ToolType::None) {
            auto &digger = mComponents.emplace<DiggerItemComponent>();
            digger.mToolType = mToolType;
            digger.mTier = mToolTier;
            digger.mHarvestLevel = mHarvestLevel;
        }

        if (mArmorSlot != ArmorSlot::None) {
            auto &armor = mComponents.emplace<ArmorItemComponent>();
            armor.mSlot = mArmorSlot;
            armor.mTier = mArmorTier;
            armor.mPoints = mArmorPoints;
            armor.mToughness = mArmorToughness;

            auto &wearable = mComponents.emplace<WearableItemComponent>();
            wearable.mSlot = mArmorSlot;
        }

        if (mNutrition > 0) {
            auto &food = mComponents.emplace<FoodItemComponent>();
            food.mNutrition = mNutrition;
            food.mSaturation = mSaturation;
        }

        const int cooldown = ItemCooldowns::getDuration(mIdentifier);
        if (cooldown > 0) {
            auto &component = mComponents.emplace<CooldownItemComponent>();
            component.mCategory = ItemCooldowns::getCategory(mIdentifier);
            component.mDurationTicks = cooldown;
        }

        if (mIdentifier.find("netherite") != std::string::npos || mIdentifier == "minecraft:ancient_debris")
            mComponents.emplace<FireResistantItemComponent>();
    }

    int32_t mTypeId;
    std::string mIdentifier;
    std::string mName;
    int32_t mMaxStackSize;
    bool mIsBlock;
    Block mBlock;
    int32_t mMaxDurability = 0;
    ToolType mToolType = ToolType::None;
    int32_t mToolTier = 0;
    int32_t mHarvestLevel = 0;
    int32_t mAttackDamage = 0;
    ArmorSlot mArmorSlot = ArmorSlot::None;
    int32_t mArmorTier = 0;
    int32_t mArmorPoints = 0;
    int32_t mArmorToughness = 0;
    int32_t mNutrition = 0;
    float mSaturation = 0.0f;
    ItemComponents mComponents;
};

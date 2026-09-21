#pragma once

#include "Actor/ActorAttributes.h"
#include "Actor/ActorFlags.h"
#include "Actor/MobEffect.h"
#include "Actor/ExperienceManager.h"
#include "Core/Math/Vector3f.h"
#include "Level/Dimension.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

class Actor {
public:
    explicit Actor(uint64_t runtimeId);

    virtual ~Actor() = default;

    virtual const char *getIdentifier() const = 0;

    uint64_t getRuntimeId() const { return mRuntimeId; }

    int64_t getUniqueId() const { return (int64_t) mRuntimeId; }

    const Vector3f &getPosition() const { return mPosition; }

    void setPosition(const Vector3f &position) { mPosition = position; }

    void teleport(const Vector3f &position);

    DimensionType getDimension() const { return mDimension; }

    void setDimension(DimensionType dimension) { mDimension = dimension; }

    int32_t getPortalCooldown() const { return mPortalCooldown; }

    void setPortalCooldown(int32_t ticks) { mPortalCooldown = ticks; }

    int64_t getLastPortalTick() const { return mLastPortalTick; }

    void setLastPortalTick(int64_t tick) { mLastPortalTick = tick; }

    int32_t getPortalTicks() const { return mPortalTicks; }

    void setPortalTicks(int32_t ticks) { mPortalTicks = ticks; }

    const Vector3f &getRotation() const { return mRotation; }

    void setRotation(const Vector3f &rotation) { mRotation = rotation; }

    const Vector3f &getMotion() const { return mMotion; }

    void setMotion(const Vector3f &motion) { mMotion = motion; }

    int64_t getVehicleId() const { return mVehicleId; }

    bool isRiding() const { return mVehicleId != 0; }

    const std::vector<int64_t> &getPassengers() const { return mPassengers; }

    bool hasPassengers() const { return !mPassengers.empty(); }

    void _setVehicleId(int64_t vehicleId) { mVehicleId = vehicleId; }

    void _addPassenger(int64_t passengerId) { mPassengers.push_back(passengerId); }

    void _removePassenger(int64_t passengerId) {
        mPassengers.erase(std::remove(mPassengers.begin(), mPassengers.end(), passengerId),
                          mPassengers.end());
    }

    void _clearPassengers() { mPassengers.clear(); }

    ActorFlags &getFlags() { return mFlags; }

    const ActorFlags &getFlags() const { return mFlags; }

    ActorAttributes &getAttributes() { return mAttributes; }

    const ActorAttributes &getAttributes() const { return mAttributes; }

    bool isOnFire() const { return mFlags.get(ActorFlag::OnFire); }

    void setOnFire(bool onFire = true);

    void setFireTicks(int fireTicks);

    void extinguish();

    int getFireTicks() const { return mFireTicks; }

    bool tickFire();

    bool isOnGround() const { return mOnGround; }

    void setOnGround(bool onGround) { mOnGround = onGround; }

    float getFallDistance() const { return mFallDistance; }

    float getHighestPosition() const { return mHighestPosition; }

    void setHighestPosition(float highestPosition) { mHighestPosition = highestPosition; }

    void updateFallDistance() { mFallDistance = mHighestPosition - mPosition.y; }

    void resetFallDistance() {
        mFallDistance = 0.0f;
        mHighestPosition = mPosition.y;
    }

    ExperienceManager &getExperience() { return mExperience; }

    const ExperienceManager &getExperience() const { return mExperience; }

    void syncExperience() { mExperience.applyTo(mAttributes); }

    void setXpAndProgress(int level, float progress);

    void addXp(int amount);

    void addXpLevels(int amount);

    float getHealth() const;

    void setHealth(float health);

    float getMaxHealth() const;

    void setMaxHealth(float maxHealth);

    bool isAlive() const;

    int getNoDamageTicks() const { return mNoDamageTicks; }

    void setNoDamageTicks(int ticks) { mNoDamageTicks = std::max(0, ticks); }

    float getLastDamageAmount() const { return mLastDamageAmount; }

    void setLastDamageAmount(float amount) { mLastDamageAmount = std::max(0.0f, amount); }

    int getAttackTime() const { return mAttackTime; }

    void setAttackTime(int ticks) { mAttackTime = std::max(0, ticks); }

    void tickCombat(int tickDiff = 1);

    void knockBack(float x, float z, float force = 0.4f, float verticalLimit = 0.4f);

    float getFood() const;

    float getMaxFood() const;

    void setFood(float food);

    void addFood(float amount);

    bool isHungry() const;

    bool consumeStarveDamage() {
        const bool starving = mPendingStarveDamage;
        mPendingStarveDamage = false;
        return starving;
    }

    bool canSprint() const { return getFood() > 6.0f; }

    bool canEat() const;

    float getSaturation() const;

    void setSaturation(float saturation);

    void addSaturation(float amount);

    float getExhaustion() const;

    void setExhaustion(float exhaustion);

    void exhaust(float amount);

    int getFoodTickTimer() const { return mFoodTickTimer; }

    void setFoodTickTimer(int foodTickTimer);

    bool isHungerEnabled() const { return mHungerEnabled; }

    void setHungerEnabled(bool enabled) { mHungerEnabled = enabled; }

    void consumeFood(int nutrition, float saturation);

    void resetHungerAndExperience();

    bool tickHunger(int tickDiff, int difficulty, bool naturalRegeneration);

    bool isDead() const { return mIsDead; }

    void setDead(bool isDead) { mIsDead = isDead; }

    void kill();

    float reduceHealth(float amount);

    float heal(float amount);

    MobEffectManager &getEffects() { return mEffects; }

    const MobEffectManager &getEffects() const { return mEffects; }

    bool addEffect(const MobEffectInstance &effect) { return mEffects.add(effect); }

    bool removeEffect(MobEffectId id) { return mEffects.remove(id); }

    bool hasEffect(MobEffectId id) const { return mEffects.has(id); }

    const MobEffectInstance *getEffect(MobEffectId id) const { return mEffects.get(id); }

    int64_t getVisibleEffectsData() const;

    bool refreshVisibleEffects();

    bool isUndead() const;

    bool isArthropod() const;

    bool isFireImmune() const;

    float getMeleeEnchantmentBonus(const ItemStack &weapon) const;

    void onMeleeEnchantmentHit(const ItemStack &weapon);

    bool tickEffects(int32_t tickDiff = 1) { return mEffects.tick(tickDiff); }

    float getMovementSpeedMultiplier() const { return mEffects.movementSpeedMultiplier(); }

    float getJumpVelocityMultiplier() const { return mEffects.jumpVelocityMultiplier(); }

    float computeFallDamage() const;

protected:
    static const float EXHAUSTION_PER_UNIT;
    static const float FALL_DAMAGE_THRESHOLD;
    static const int FOOD_TICK_PERIOD = 80;

    uint64_t mRuntimeId;
    int64_t mVehicleId = 0;
    std::vector<int64_t> mPassengers;
    Vector3f mPosition;
    DimensionType mDimension = DimensionType::Overworld;
    int32_t mPortalCooldown = 0;
    int32_t mPortalTicks = 0;
    int64_t mLastPortalTick = -1;
    int64_t mSentVisibleEffects = 0;
    Vector3f mRotation;
    Vector3f mMotion;
    ActorFlags mFlags;
    ActorAttributes mAttributes;
    bool mOnGround = false;
    int mFireTicks = 0;
    float mFallDistance = 0.0f;
    float mHighestPosition = 0.0f;
    ExperienceManager mExperience;
    MobEffectManager mEffects;
    int mFoodTickTimer = 0;
    bool mHungerEnabled = true;
    int mDifficulty = 2;
    bool mPendingStarveDamage = false;
    bool mIsDead = false;
    int mNoDamageTicks = 0;
    int mAttackTime = 0;
    float mLastDamageAmount = 0.0f;
};

#pragma once

#include "World/Actor/ActorAttributes.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ExperienceManager.h"
#include "Core/Math/Vector3f.h"
#include "Core/NBT/Tag.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Vibration/MovementGameEvents.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

enum class Difficulty : int;
class EventBus;
class ServerNetworkHandler;
class Level;

// the EntityHealCause values
enum class HealCause { Heal, Regeneration, SelfHeal, TotemOfUndying };

// the game's empty tag components for actor kinds, one bit each: a kind is checked without a cast
enum class ActorTypeFlag : uint8_t {
    PlayerComponent = 1 << 0,
    ItemActorFlagComponent = 1 << 1,
    MobFlagComponent = 1 << 2,
    PaintingFlagComponent = 1 << 3
};

class Actor {
public:
    explicit Actor(uint64_t runtimeId);

    virtual ~Actor() = default;

    virtual const char *getIdentifier() const = 0;

    virtual std::string getName() const;

    virtual bool isPlayer() const {
        return false;
    }

    bool hasTypeFlag(ActorTypeFlag flag) const {
        return (mTypeFlags & (uint8_t) flag) != 0;
    }

    // falling blocks and actors with can_stand_on_powder_snow or leather boots
    virtual bool canStandOnPowderSnow() const {
        return false;
    }

    // added to the knockback resistance attribute by the armor worn
    virtual float getArmorKnockbackResistance() const {
        return 0.0f;
    }

    // item entities and projectiles meet a snow layer at its look
    virtual bool collidesAtSnowLayerLook() const {
        return false;
    }

    // every minecart: the actors detector rails sense
    virtual bool isMinecart() const {
        return false;
    }

    // AutonomousActorComponent: a player keeps it once sent, out of the actor range too
    virtual bool isAutonomous() const {
        return false;
    }

    bool catchFireFrom(const Actor &attacker, Difficulty difficulty);

    // minecraft:block_movement_slowdown_immunity: the block leaves this actor's speed alone
    virtual bool ignoresMovementSlowdown(const std::string &blockName) const {
        (void) blockName;
        return false;
    }

    uint64_t getRuntimeId() const {
        return mRuntimeId;
    }

    int64_t getUniqueId() const {
        return mUniqueId;
    }

    void setUniqueId(int64_t uniqueId) {
        mUniqueId = uniqueId;
    }

    bool hasAssignedUniqueId() const {
        return mUniqueId != (int64_t) mRuntimeId;
    }

    const Vector3f &getPosition() const {
        return mPosition;
    }

    void setPosition(const Vector3f &position) {
        mPosition = position;
    }

    void teleport(const Vector3f &position);

    DimensionType getDimension() const {
        return mDimension;
    }

    void setDimension(DimensionType dimension) {
        mDimension = dimension;
    }

    uint32_t getWorldId() const {
        return mWorldId;
    }

    // dimension and world change together
    void moveToLevel(const Level &level);

    // the bus of the server whose level the actor was last moved into; null before that
    EventBus *getEventBus() const {
        return mEventBus;
    }

    bool isIn(const Level &level) const;

    bool sharesLevelWith(const Actor &other) const {
        return mDimension == other.mDimension && mWorldId == other.mWorldId;
    }

    int32_t getPortalCooldown() const {
        return mPortalCooldown;
    }

    void setPortalCooldown(int32_t ticks) {
        mPortalCooldown = ticks;
    }

    int64_t getLastPortalTick() const {
        return mLastPortalTick;
    }

    void setLastPortalTick(int64_t tick) {
        mLastPortalTick = tick;
    }

    int32_t getPortalTicks() const {
        return mPortalTicks;
    }

    void setPortalTicks(int32_t ticks) {
        mPortalTicks = ticks;
    }

    const Vector3f &getRotation() const {
        return mRotation;
    }

    void setRotation(const Vector3f &rotation) {
        mRotation = rotation;
    }

    const Vector3f &getMotion() const {
        return mMotion;
    }

    void setMotion(const Vector3f &motion) {
        mMotion = motion;
    }

    int64_t getVehicleId() const {
        return mVehicleId;
    }

    bool isRiding() const {
        return mVehicleId != 0;
    }

    const std::vector<int64_t> &getPassengers() const {
        return mPassengers;
    }

    bool hasPassengers() const {
        return !mPassengers.empty();
    }

    void _setVehicleId(int64_t vehicleId) {
        mVehicleId = vehicleId;
    }

    void _addPassenger(int64_t passengerId) {
        mPassengers.push_back(passengerId);
    }

    void _removePassenger(int64_t passengerId) {
        mPassengers.erase(std::remove(mPassengers.begin(), mPassengers.end(), passengerId), mPassengers.end());
    }

    void _clearPassengers() {
        mPassengers.clear();
    }

    ActorFlags &getFlags() {
        return mFlags;
    }

    const ActorFlags &getFlags() const {
        return mFlags;
    }

    float getNameplateRenderDistance() const {
        return mNameplateRenderDistance;
    }

    void setNameplateRenderDistance(float distance) {
        mNameplateRenderDistance = distance;
    }

    ActorAttributes &getAttributes() {
        return mAttributes;
    }

    const ActorAttributes &getAttributes() const {
        return mAttributes;
    }

    bool isOnFire() const {
        return mFlags.get(ActorFlag::OnFire);
    }

    void setOnFire(bool onFire = true);

    void setFireTicks(int fireTicks);

    void extinguish();

    int getFireTicks() const {
        return mFireTicks;
    }

    bool tickFire();

    bool isOnGround() const {
        return mOnGround;
    }

    void setOnGround(bool onGround) {
        mOnGround = onGround;
    }

    bool hasHorizontalCollision() const {
        return mHorizontalCollision;
    }

    void setHorizontalCollision(bool collision) {
        mHorizontalCollision = collision;
    }

    void makeStuckInBlock(const Vector3f &multiplier) {
        mStuckMultiplier = multiplier;
        mStuck = true;
    }

    bool takeStuckMultiplier(Vector3f &multiplier) {
        if (!mStuck)
            return false;

        multiplier = mStuckMultiplier;
        mStuck = false;
        return true;
    }

    float getFallDistance() const {
        return mFallDistance;
    }

    float getHighestPosition() const {
        return mHighestPosition;
    }

    void setHighestPosition(float highestPosition) {
        mHighestPosition = highestPosition;
    }

    void updateFallDistance() {
        mFallDistance = mHighestPosition - mPosition.y;
    }

    void resetFallDistance() {
        mFallDistance = 0.0f;
        mHighestPosition = mPosition.y;
    }

    ExperienceManager &getExperience() {
        return mExperience;
    }

    const ExperienceManager &getExperience() const {
        return mExperience;
    }

    void syncExperience() {
        mExperience.applyTo(mAttributes);
    }

    void setXpAndProgress(int level, float progress);

    void addXp(int amount);

    void addXpLevels(int amount);

    float getHealth() const;

    void setHealth(float health);

    float getMaxHealth() const;

    void setMaxHealth(float maxHealth);

    bool isAlive() const;

    // the game freezes mobs only, players included
    virtual bool canFreeze() const {
        return false;
    }

    // instant effects included: a refused effect is neither added nor applied
    virtual bool canBeAffected(MobEffectId id) const {
        (void) id;
        return true;
    }

    void markInFreezingBlock(int64_t tick) {
        mFreezingBlockTick = tick;
    }

    bool touchedFreezingBlock(int64_t tick) const {
        return mFreezingBlockTick == tick;
    }

    int getNoDamageTicks() const {
        return mNoDamageTicks;
    }

    void setNoDamageTicks(int ticks) {
        mNoDamageTicks = std::max(0, ticks);
    }

    static constexpr int SPAWN_INVULNERABILITY_TICKS = 60;

    bool isSpawnInvulnerable() const {
        return mSpawnInvulnerabilityTicks > 0;
    }

    void grantSpawnInvulnerability() {
        mSpawnInvulnerabilityTicks = SPAWN_INVULNERABILITY_TICKS;
    }

    void tickSpawnInvulnerability() {
        if (mSpawnInvulnerabilityTicks > 0)
            --mSpawnInvulnerabilityTicks;
    }

    float getLastDamageAmount() const {
        return mLastDamageAmount;
    }

    void setLastDamageAmount(float amount) {
        mLastDamageAmount = std::max(0.0f, amount);
    }

    int getAttackTime() const {
        return mAttackTime;
    }

    void setAttackTime(int ticks) {
        mAttackTime = std::max(0, ticks);
    }

    void tickCombat(int tickDiff = 1);

    // a sculk catalyst handles each death once
    bool isHandledBySculkCatalyst() const {
        return mHandledBySculkCatalyst;
    }

    void setHandledBySculkCatalyst() {
        mHandledBySculkCatalyst = true;
    }

    MovementGameEventState &getMovementGameEventState() {
        return mMovementGameEventState;
    }

    // the experience this death drops, taken: it is not dropped any more; 0 when there is none to take
    virtual int32_t takeDeathExperience(ServerNetworkHandler &owner) {
        (void) owner;
        return 0;
    }

    void knockBack(float x, float z, float force = 0.4f, float verticalLimit = 0.4f);

    // the push a plugin set for the hit being dealt: the next knockback this tick uses it in place of its own, the
    // others of that hit are skipped; tickCombat forgets it
    void setKnockbackOverride(float horizontal, float vertical) {
        mKnockbackOverride = true;
        mKnockbackOverrideUsed = false;
        mKnockbackHorizontal = horizontal;
        mKnockbackVertical = vertical;
    }

    void clearKnockbackOverride() {
        mKnockbackOverride = false;
        mKnockbackOverrideUsed = false;
    }

    // the game's knockback with its own powers, as minecraft:apply_knockback_rules sets them; (x, z) points away
    // from the attacker
    void knockBack(float x, float z, float horizontalPower, float verticalPower, float verticalCap, float slowdownScale);

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

    bool canSprint() const {
        return getFood() > 6.0f;
    }

    bool canEat() const;

    float getSaturation() const;

    void setSaturation(float saturation);

    void addSaturation(float amount);

    float getExhaustion() const;

    void setExhaustion(float exhaustion);

    void exhaust(float amount);

    int getFoodTickTimer() const {
        return mFoodTickTimer;
    }

    void setFoodTickTimer(int foodTickTimer);

    bool isHungerEnabled() const {
        return mHungerEnabled;
    }

    void setHungerEnabled(bool enabled) {
        mHungerEnabled = enabled;
    }

    void consumeFood(int nutrition, float saturation);

    void resetHungerAndExperience();

    bool tickHunger(int tickDiff, int difficulty, bool naturalRegeneration);

    bool isDead() const {
        return mIsDead;
    }

    void setDead(bool isDead) {
        mIsDead = isDead;
    }

    void kill();

    // takes the amount as is: resistance belongs to the caller, see resistDamage
    float reduceHealth(float amount);

    float resistDamage(float amount) const;

    float heal(float amount, HealCause cause = HealCause::Heal);

    // the filter may rewrite the amount and returns false to cancel; the listener sees what was healed
    using HealFilter = std::function<bool(Actor &, float &, HealCause)>;
    using HealListener = std::function<void(Actor &, float, HealCause)>;

    static void setHealHooks(HealFilter filter, HealListener listener) {
        sHealFilter = std::move(filter);
        sHealListener = std::move(listener);
    }

    // every health write through setHealth, reduceHealth, heal and kill
    using HealthListener = std::function<void(Actor &, float, float)>;

    static void setHealthListener(HealthListener listener) {
        sHealthListener = std::move(listener);
    }

    MobEffectManager &getEffects() {
        return mEffects;
    }

    const MobEffectManager &getEffects() const {
        return mEffects;
    }

    bool addEffect(const MobEffectInstance &effect) {
        return mEffects.add(effect);
    }

    bool removeEffect(MobEffectId id) {
        return mEffects.remove(id);
    }

    bool hasEffect(MobEffectId id) const {
        return mEffects.has(id);
    }

    const MobEffectInstance *getEffect(MobEffectId id) const {
        return mEffects.get(id);
    }

    const std::vector<std::string> &getTags() const {
        return mTags;
    }

    bool hasTag(const std::string &tag) const;

    bool addTag(const std::string &tag);

    bool removeTag(const std::string &tag);

    void saveTags(Tag &data) const;

    void loadTags(const Tag &data);

    int64_t getVisibleEffectsData() const;

    bool refreshVisibleEffects();

    bool isUndead() const;

    bool isArthropod() const;

    bool isFireImmune() const;

    float getMeleeEnchantmentBonus(const ItemStack &weapon) const;

    void onMeleeEnchantmentHit(const ItemStack &weapon);

    bool tickEffects(int32_t tickDiff = 1) {
        return mEffects.tick(tickDiff);
    }

    float getMovementSpeedMultiplier() const {
        return mEffects.movementSpeedMultiplier();
    }

    float computeFallDamage(float distance) const;

protected:
    bool mHandledBySculkCatalyst = false;
    MovementGameEventState mMovementGameEventState;
    void _changeFood(float food);

    static inline HealFilter sHealFilter;
    static inline HealListener sHealListener;
    static inline HealthListener sHealthListener;

    void _healthChanged(float previous) {
        if (sHealthListener && getHealth() != previous)
            sHealthListener(*this, previous, getHealth());
    }

    static const float EXHAUSTION_PER_UNIT;
    static const float FALL_DAMAGE_THRESHOLD;
    static const int FOOD_TICK_PERIOD = 80;

    // set by the constructor of each kind
    void addTypeFlag(ActorTypeFlag flag) {
        mTypeFlags |= (uint8_t) flag;
    }

    uint8_t mTypeFlags = 0;
    uint64_t mRuntimeId;
    int64_t mUniqueId;
    int64_t mVehicleId = 0;
    std::vector<int64_t> mPassengers;
    Vector3f mPosition;
    DimensionType mDimension = DimensionType::Overworld;
    uint32_t mWorldId = 0;
    EventBus *mEventBus = nullptr;
    int32_t mPortalCooldown = 0;
    int32_t mPortalTicks = 0;
    int64_t mLastPortalTick = -1;
    int64_t mSentVisibleEffects = 0;
    std::vector<std::string> mTags;
    Vector3f mRotation;
    Vector3f mMotion;
    ActorFlags mFlags;
    // data 140, defined at 64 for every actor
    float mNameplateRenderDistance = 64.0f;
    ActorAttributes mAttributes;
    bool mOnGround = false;
    bool mHorizontalCollision = false;
    bool mStuck = false;
    Vector3f mStuckMultiplier;
    int mFireTicks = 0;
    float mFallDistance = 0.0f;
    float mHighestPosition = 0.0f;
    ExperienceManager mExperience;
    MobEffectManager mEffects;
    int mFoodTickTimer = 0;
    // never reset: the peaceful food and health cadence
    int mHungerTicks = 0;
    bool mHungerEnabled = true;
    int mDifficulty = 2;
    bool mPendingStarveDamage = false;
    bool mIsDead = false;
    int64_t mFreezingBlockTick = -1;
    int mNoDamageTicks = 0;
    int mSpawnInvulnerabilityTicks = 0;
    int mAttackTime = 0;
    float mLastDamageAmount = 0.0f;
    bool mKnockbackOverride = false;
    bool mKnockbackOverrideUsed = false;
    float mKnockbackHorizontal = 0.0f;
    float mKnockbackVertical = 0.0f;
};

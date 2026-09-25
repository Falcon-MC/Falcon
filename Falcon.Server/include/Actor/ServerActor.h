#pragma once

#include "Actor/Actor.h"
#include "Actor/ActorSize.h"
#include "Actor/DynamicPropertyValue.h"
#include "Actor/Movement/PhysicsComponent.h"
#include "Core/Math/Vector3f.h"
#include "Core/NBT/Tag.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CustomActorDefinition;
class ServerNetworkHandler;
class ServerPlayer;

struct ProjectileData {
    float mBaseDamage = 0.0f;
    bool mCritical = false;
    int32_t mPunchLevel = 0;
    int32_t mFlameTicks = 0;
    int32_t mLoyaltyLevel = 0;
    int32_t mImpalingLevel = 0;
    int32_t mPiercingLevel = 0;
    int32_t mLootingLevel = 0;
    bool mChanneling = false;
    bool mReturning = false;
    bool mHadCollision = false;
    bool mPickupCreativeOnly = false;
    ItemStack mPickupItem;
    int32_t mFavoredSlot = -1;
    Tag mFireworkData;
    int32_t mFireworkLifetime = 0;
    int32_t mFireworkAge = 0;
    bool mFireworkAttached = false;
};

class ServerActor : public Actor {
public:
    static constexpr float DEFAULT_WIDTH = 0.6f;

    static constexpr float DEFAULT_HEIGHT = 1.8f;

    ServerActor(uint64_t runtimeId, const std::string &identifier);

    ~ServerActor() override = default;

    virtual void tick(ServerNetworkHandler &owner);

    virtual ActorSize getSize() const { return ActorSize{DEFAULT_WIDTH, DEFAULT_HEIGHT}; }

    virtual bool isExpired() const { return false; }

    virtual bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
        (void) owner;
        (void) amount;
        (void) source;
        return false;
    }

    virtual void onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
        (void) owner;
        (void) attacker;
    }

    virtual float getBaseOffset() const {
        return 0.0f;
    }

    virtual Vector3f getSeatOffset() const {
        return Vector3f(0.0f, 0.0f, 0.0f);
    }

    virtual bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
        (void) owner;
        (void) player;
        return false;
    }

    virtual void onStruckByLightning(ServerNetworkHandler &owner) {
        (void) owner;
    }

    virtual bool isInvulnerable() const {
        return false;
    }

    virtual void fillSpawnMetadata(EntityDataMap &metadata) const { (void) metadata; }

    virtual int32_t getDeathDuration() const {
        return 25;
    }

    virtual EntityEventType getDeathEvent() const {
        return EntityEventType::DeathAnimation;
    }

    /**
     * Damages the actor. On death the drops use lootingLevel; a negative level reads it from the
     * item source is holding, which is right for melee but not for a projectile, whose level is
     * captured when it is launched.
     */
    bool hurt(ServerNetworkHandler &owner, float amount, Actor *attacker, int32_t lootingLevel = -1);

    virtual void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0);

    void tickFire(ServerNetworkHandler &owner);

    void tickSunlightBurn(ServerNetworkHandler &owner);

    virtual PhysicsComponent getPhysics() const;

    virtual bool hasGravity() const {
        return true;
    }

    virtual bool isPushable() const {
        return true;
    }

    virtual bool burnsInDaylight() const {
        return false;
    }

    bool needsMovementSync() const;

    void markMovementSynced();

    const char *getIdentifier() const override { return mIdentifier.c_str(); }

    const std::string &getTypeId() const { return mIdentifier; }

    void setDefinition(const CustomActorDefinition *definition) { mDefinition = definition; }

    const CustomActorDefinition *getDefinition() const { return mDefinition; }

    void setIntProperty(const std::string &name, int32_t value) { mIntProperties[name] = value; }

    void setFloatProperty(const std::string &name, float value) { mFloatProperties[name] = value; }

    bool hasIntProperty(const std::string &name) const { return mIntProperties.count(name) != 0; }

    bool hasFloatProperty(const std::string &name) const { return mFloatProperties.count(name) != 0; }

    int32_t getIntProperty(const std::string &name, int32_t fallback = 0) const;

    float getFloatProperty(const std::string &name, float fallback = 0.0f) const;

    std::unordered_map<std::string, int32_t> &getIntProperties() { return mIntProperties; }

    std::unordered_map<std::string, float> &getFloatProperties() { return mFloatProperties; }

    std::unordered_map<std::string, DynamicPropertyValue> &getDynamicProperties() { return mDynamicProperties; }

    void setOwnerUniqueId(int64_t owner) { mOwnerUniqueId = owner; }

    int64_t getOwnerUniqueId() const { return mOwnerUniqueId; }

    void setOwnerPlayerHandle(uint32_t handle) { mOwnerPlayerHandle = handle; }

    uint32_t getOwnerPlayerHandle() const { return mOwnerPlayerHandle; }

    bool hasOwnerPlayer() const { return mOwnerPlayerHandle != 0xFFFFFFFF; }

    bool isProjectile() const { return mIsProjectile; }

    void setProjectile(bool value) { mIsProjectile = value; }

    ProjectileData &getProjectileData() { return mProjectileData; }

    const ProjectileData &getProjectileData() const { return mProjectileData; }

    int32_t getLifetimeTicks() const { return mLifetimeTicks; }

    void addLifetimeTick() { mLifetimeTicks++; }

    void setExperienceValue(int32_t value) { mExperienceValue = value; }

    int32_t getExperienceValue() const { return mExperienceValue; }

    void setPickupDelay(int32_t delay) { mPickupDelay = delay; }

    int32_t getPickupDelay() const { return mPickupDelay; }

    void decrementPickupDelay() {
        if (mPickupDelay > 0)
            mPickupDelay--;
    }

    int32_t getDeathTicks() const { return mDeathTicks; }

    void addDeathTick() { mDeathTicks++; }

    const std::string &getNameTag() const { return mNameTag; }

    std::string getName() const override {
        return mNameTag.empty() ? Actor::getName() : mNameTag;
    }

    void setNameTag(const std::string &nameTag) { mNameTag = nameTag; }

    bool isPersistent() const {
        return mPersistent || !mNameTag.empty();
    }

    void setPersistent(bool persistent) {
        mPersistent = persistent;
    }

    int32_t getFarFromPlayerTicks() const {
        return mFarFromPlayerTicks;
    }

    void setFarFromPlayerTicks(int32_t ticks) {
        mFarFromPlayerTicks = ticks;
    }

    virtual bool shouldSave() const { return isAlive() && !mIsProjectile && !hasOwnerPlayer(); }

    virtual Tag saveNbt() const;

    virtual void loadNbt(const Tag &data);

private:
    std::string mIdentifier;
    Vector3f mSyncedPosition;
    Vector3f mSyncedRotation;
    bool mMovementSynced = false;
    const CustomActorDefinition *mDefinition = nullptr;
    bool mIsProjectile = false;
    int64_t mOwnerUniqueId = -1;
    uint32_t mOwnerPlayerHandle = 0xFFFFFFFF;
    int32_t mLifetimeTicks = 0;
    int32_t mDeathTicks = 0;
    int32_t mExperienceValue = 0;
    int32_t mPickupDelay = 0;
    ProjectileData mProjectileData;
    std::string mNameTag;
    bool mPersistent = true;
    int32_t mFarFromPlayerTicks = 0;

    std::unordered_map<std::string, int32_t> mIntProperties;
    std::unordered_map<std::string, float> mFloatProperties;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
};

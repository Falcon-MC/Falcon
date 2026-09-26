#pragma once

#include "Actor/AI/Control/BodyControl.h"
#include "Actor/AI/Control/JumpControl.h"
#include "Actor/AI/Control/LookControl.h"
#include "Actor/AI/Control/MoveControl.h"
#include "Actor/AI/Goal/GoalSelector.h"
#include "Actor/AI/Navigation/PathNavigation.h"
#include "Actor/ActorCategory.h"
#include "Actor/ActorFlags.h"
#include "Actor/ActorSize.h"
#include "Actor/Definition/LookedAtSensor.h"
#include "Actor/Mob/MobEntitySpawner.h"
#include "Actor/Mob/MobEquipment.h"
#include "Actor/Movement/RideControlSystem.h"
#include "Actor/ServerActor.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3i.h"
#include "Server/PropertiesSettings.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class Level;
class LootTable;
class ServerNetworkHandler;
class ServerPlayer;

class MobActor : public ServerActor {
public:
    MobActor(uint64_t runtimeId, const std::string &identifier);

    virtual ActorCategory getCategory() const = 0;

    virtual bool preventsSleep() const {
        return false;
    }

    ActorSize getSize() const override = 0;

    virtual float getDefaultMaxHealth() const = 0;

    virtual int getExperienceDrop() const { return 0; }

    bool isExpired() const override {
        return mTransformed || mDespawned;
    }

    void despawn() {
        mDespawned = true;
    }

    virtual const LootTable *getLootTable() const;

    virtual float resolveMaxHealth(Difficulty difficulty) const;

    virtual void finalizeSpawn() {
    }

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    void applyDefaults(Difficulty difficulty);

    static int randomRange(int minimum, int maximum);

    void dropLoot(ServerNetworkHandler &owner, Level &level, const ServerPlayer *killer, int32_t lootingLevel) const;

    void tick(ServerNetworkHandler &owner) override;

    MoveControl &getMoveControl() {
        return mMoveControl;
    }

    LookControl &getLookControl() {
        return mLookControl;
    }

    JumpControl &getJumpControl() {
        return mJumpControl;
    }

    PathNavigation &getNavigation() {
        return mNavigation;
    }

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    float absorbDamage(float amount, const ActorDamageSource &source) const override {
        return mEquipment.absorbDamage(amount, source);
    }

    virtual float getAttackDamage(Difficulty difficulty) const;

    const json::Value *getDefinition() const;

    const json::Value *getComponent(const std::string &name) const;

    const std::unordered_map<std::string, const json::Value *> &getComponents() const;

    std::vector<std::string> getFamilies() const;

    float getMovementSpeed() const;

    bool hasComponentGroup(const std::string &group) const;

    void addComponentGroup(const std::string &group);

    void removeComponentGroup(const std::string &group);

    void fireEvent(ServerNetworkHandler &owner, const std::string &event, Actor *other = nullptr);

    void playDefinitionSound(ServerNetworkHandler &owner, const std::string &sound);

    void setParent(uint64_t runtimeId) {
        mParentRuntimeId = runtimeId;
    }

    uint64_t getParentRuntimeId() const {
        return mParentRuntimeId;
    }

    void setSpawnEvent(const std::string &event) {
        mSpawnEvent = event;
    }

    void setHomePosition(const Vector3f &position) {
        mHomePosition = position;
        mHasHome = true;
    }

    bool hasHome() const {
        return mHasHome;
    }

    const Vector3f &getHomePosition() const {
        return mHomePosition;
    }

    void setEventBlock(const Vector3i &position) {
        mEventBlock = position;
        mHasEventBlock = true;
    }

    void clearEventBlock() {
        mHasEventBlock = false;
    }

    bool hasEventBlock() const {
        return mHasEventBlock;
    }

    const Vector3i &getEventBlock() const {
        return mEventBlock;
    }

    bool fireBlockEvent(ServerNetworkHandler &owner, const Vector3i &position, const std::string &event);

    void markBorn();

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    bool isInLove() const {
        return mLoveTicks > 0;
    }

    void finishBreeding(ServerNetworkHandler &owner);

    int32_t getVariant() const;

    void setVariant(int32_t variant);

    void inheritVariant(const MobActor &firstParent, const MobActor &secondParent);

    bool isTamed() const {
        return mOwnerId != NO_OWNER || !mLegacyOwnerName.empty();
    }

    int64_t getOwnerId() const {
        return mOwnerId;
    }

    bool isOwnedBy(const Actor &actor) const {
        return mOwnerId != NO_OWNER && actor.getUniqueId() == mOwnerId;
    }

    static constexpr int64_t NO_OWNER = -1;

    bool isSitting() const {
        return mSitting;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    int64_t getLastHurtTick() const {
        return mLastHurtTick;
    }

    uint64_t getLastHurtBy() const {
        return mLastHurtBy;
    }

    uint32_t getHurtCount() const {
        return mHurtCount;
    }

    bool setTarget(ServerNetworkHandler &owner, uint64_t runtimeId);

    void clearTarget();

    Actor *getTarget(ServerNetworkHandler &owner) const;

    bool canTarget(const Actor &actor) const;

    float distanceSquaredTo(const Actor &other) const;

    static ServerPlayer *findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId);

    static Actor *findActor(ServerNetworkHandler &owner, uint64_t runtimeId);

    MobEquipment &getEquipment() {
        return mEquipment;
    }

    Vector3f getSeatOffset(size_t index, size_t passengerCount) const override;

    Vector3f getDismountPosition(size_t index, size_t passengerCount) const override;

    void onPassengerAdded(ServerNetworkHandler &owner, Actor &passenger) override;

    void onPassengerRemoved(ServerNetworkHandler &owner, Actor &passenger) override;

    bool isMountTaming() const;

    void attemptMountTame(ServerNetworkHandler &owner, ServerPlayer &rider);

    float getJumpStrength() const;

    virtual float getRideSprintMultiplier() const {
        return 1.0f;
    }

    virtual bool canSpawnNaturally(Level &level, const Vector3i &position, int32_t biomeId, int32_t light,
                                   std::mt19937 &random) const {
        (void) level;
        (void) position;
        (void) biomeId;
        (void) light;
        (void) random;
        return true;
    }

    RideControlState &getRideControl() {
        return mRideControl;
    }

protected:
    virtual void registerGoals(GoalSelector &goalSelector) {
        (void) goalSelector;
    }

    void tickControls(ServerNetworkHandler &owner);

private:
    friend class RideControlSystem;

    void _rebuildComponents() const;

    void _registerGoals(ServerNetworkHandler &owner);

    void _syncBody(ServerNetworkHandler &owner);

    void _markComponentsChanged();

    void _tickLifecycle(ServerNetworkHandler &owner);

    void _tickSensors(ServerNetworkHandler &owner);

    void _tickEntitySensor(ServerNetworkHandler &owner);

    int32_t _countSensedEntities(ServerNetworkHandler &owner, const json::Value &subsensor, bool playersOnly,
                                 bool relativeRange);

    void _tickTimer(ServerNetworkHandler &owner);

    void _tickSpellEffects();

    void _tickTransformation(ServerNetworkHandler &owner);

    bool _tickInstantDespawn(ServerNetworkHandler &owner);

    int32_t _transformationAssist(ServerNetworkHandler &owner, const json::Value &delay);

    void _transform(ServerNetworkHandler &owner, const json::Value &transformation);

    void _fireComponentEvent(ServerNetworkHandler &owner, const char *component);

    void _setFlag(ServerNetworkHandler &owner, ActorFlag flag, bool value);

    bool _tryTame(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryFeedBaby(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryStartLove(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _trySit(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryInteract(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryFeedMount(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryMount(ServerNetworkHandler &owner, ServerPlayer &player);

    const json::Value *_seatFor(size_t index, size_t passengerCount) const;

    float _rolledValue(const json::Value *component, float &rolled, float fallback) const;

    const json::Value *_equippableSlot(int32_t index) const;

    const ItemStack &_equippableItem(int32_t index) const;

    void _setEquippableItem(int32_t index, ItemStack item);

    bool _equipFromHand(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &slotName);

    void _dropEquipmentSlot(ServerNetworkHandler &owner, const std::string &slotName, float yOffset);

    void _spawnLoot(ServerNetworkHandler &owner, Level &level, const std::string &path);

    std::vector<ItemStack> _rollLoot(ServerNetworkHandler &owner, const std::string &path);

    void _givePlayerItem(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack item);

    void _emitInteractParticle(ServerNetworkHandler &owner, const ServerPlayer &player, const json::Value &particle,
                               const std::string &type);

    void _appendDefinitionData(EntityDataMap &metadata) const;

    void _syncOwner(ServerNetworkHandler &owner);

    GoalSelector mGoalSelector;
    bool mTargetAcquired = false;
    bool mTargetEscaped = false;
    int32_t mLoveTicks = 0;
    int32_t mBreedCooldown = 0;
    int32_t mInteractCooldown = 0;
    int32_t mAgeTicks = 0;
    int64_t mOwnerId = NO_OWNER;
    std::string mLegacyOwnerName;
    int64_t mSyncedOwnerId = NO_OWNER;
    bool mSitting = false;
    bool mGoalsRegistered = false;
    bool mGoalsDirty = false;
    bool mBodyDirty = true;
    float mScale = 1.0f;
    bool mDefinitionStarted = false;
    bool mBorn = false;
    std::string mSpawnEvent;
    bool mEquipmentInherited = false;
    const json::Value *mSpellEffectsComponent = nullptr;
    const json::Value *mTimerComponent = nullptr;
    int32_t mTimerTicks = 0;
    const json::Value *mTransformationComponent = nullptr;
    int32_t mTransformationTicks = 0;
    bool mTransformed = false;
    uint64_t mParentRuntimeId = 0;
    Vector3f mHomePosition;
    bool mHasHome = false;
    Vector3i mEventBlock;
    bool mHasEventBlock = false;
    bool mDespawned = false;
    LookedAtSensor mLookedAtSensor;
    std::vector<std::string> mComponentGroups;
    std::vector<std::string> mGoalGroups;
    std::vector<std::string> mBodyGroups;
    bool mBodySynced = false;
    mutable const json::Value *mDefinition = nullptr;
    mutable bool mDefinitionResolved = false;
    mutable std::unordered_map<std::string, const json::Value *> mComponents;
    mutable bool mComponentsDirty = true;
    int64_t mLastHurtTick = INT64_MIN / 2;
    uint64_t mLastHurtBy = 0;
    uint32_t mHurtCount = 0;
    uint64_t mTargetRuntimeId = 0;
    PathNavigation mNavigation;
    MoveControl mMoveControl;
    LookControl mLookControl;
    JumpControl mJumpControl;
    BodyControl mBodyControl;
    MobEquipment mEquipment;
    MobEntitySpawner mEntitySpawner;
    std::vector<int32_t> mEntitySensorCooldowns;
    int32_t mTemper = 0;
    mutable float mRolledMovementSpeed = -1.0f;
    mutable float mRolledJumpStrength = -1.0f;
    RideControlState mRideControl;
};

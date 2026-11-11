#pragma once

#include "World/Actor/ActorDamageSource.h"

#include "World/Actor/AI/Control/BodyControl.h"
#include "World/Actor/AI/Control/JumpControl.h"
#include "World/Actor/AI/Control/LookControl.h"
#include "World/Actor/AI/Control/MoveControl.h"
#include "World/Actor/AI/Goal/GoalSelector.h"
#include "World/Actor/AI/Navigation/PathNavigation.h"
#include "World/Actor/ActorCategory.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/ActorSize.h"
#include "World/Actor/Definition/LookedAtSensor.h"
#include "World/Actor/Mob/Component/MobAddRider.h"
#include "World/Actor/Mob/Component/MobAdmiration.h"
#include "World/Actor/Mob/Component/MobAreaAttack.h"
#include "World/Actor/Mob/Component/MobEquipmentChanged.h"
#include "World/Actor/Mob/Component/MobExplode.h"
#include "World/Actor/Mob/Component/MobLeash.h"
#include "World/Actor/Mob/Component/MobAnger.h"
#include "World/Actor/Mob/Component/MobDweller.h"
#include "World/Actor/Mob/MobEntitySpawner.h"
#include "World/Actor/Mob/MobEquipment.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/Movement/RideControlSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/Container.h"
#include "World/Level/Block/BlockState.h"
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
    bool canFreeze() const override {
        return true;
    }

    // generated mobs only: the event their first tick fires, like villager_v2's minecraft:spawn_from_village
    static constexpr const char *TAG_SPAWN_EVENT = "SpawnEvent";

    MobActor(uint64_t runtimeId, const std::string &identifier);

    bool canStandOnPowderSnow() const override;

    virtual ActorCategory getCategory() const = 0;

    virtual bool preventsSleep() const {
        return false;
    }

    ActorSize getSize() const override = 0;

    // minecraft:collision_box of the active groups
    ActorSize definitionSize(const ActorSize &fallback) const;

    PhysicsComponent getPhysics() const override;

    bool canSwim() const;

    bool canWalk() const;

    virtual float getDefaultMaxHealth() const = 0;

    virtual int getExperienceDrop() const {
        return 0;
    }

    // a class with its own contact attack, explosion or rolled health keeps it over the definition component
    virtual bool hasOwnAreaAttack() const {
        return false;
    }

    virtual bool hasOwnExplosion() const {
        return false;
    }

    // minecraft:health of the active groups sets the maximum: off by default, the classes roll their health
    virtual bool followsDefinitionHealth() const {
        return false;
    }

    virtual bool shouldRemoveInPeaceful() const {
        return getComponent("minecraft:remove_in_peaceful") != nullptr;
    }

    int getBreedingExperience() const;

    bool isExpired() const override {
        return mTransformed || mDespawned;
    }

    void despawn() {
        mDespawned = true;
    }

    virtual const LootTable *getLootTable() const;

    virtual float resolveMaxHealth(Difficulty difficulty) const;

    virtual void finalizeSpawn() {}

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    // now instead of on the first tick; at most once
    void startDefinition(ServerNetworkHandler &owner);

    void applyDefaults(Difficulty difficulty);

    // only a loaded actor runs its pending upgrades; any other cause takes the current version (sub_140B938D0)
    void upgradeDefinition(ServerNetworkHandler &owner, bool loaded);

    static int randomRange(int minimum, int maximum);

    void dropLoot(ServerNetworkHandler &owner, Level &level, const ServerPlayer *killer, int32_t lootingLevel) const;

    void tick(ServerNetworkHandler &owner) override;

    void tickDeath(ServerNetworkHandler &owner) override;

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

    const PathNavigation &getNavigation() const {
        return mNavigation;
    }

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    void causeFallDamage(ServerNetworkHandler &owner, float distance) override;

    float absorbDamage(float amount, const ActorDamageSource &source) const override {
        return mEquipment.absorbDamage(amount, source);
    }

    bool ignoresMovementSlowdown(const std::string &blockName) const override;

    bool burnsInDaylight() const override;

    bool shieldFromSunlight() const override;

    virtual float getAttackDamage(Difficulty difficulty) const;

    const json::Value *getDefinition() const;

    const json::Value *getComponent(const std::string &name) const;

    // component names exceed the small string buffer: a temporary std::string per tick lookup allocates;
    // this reuses one key buffer
    const json::Value *getComponent(const char *name) const;

    const std::unordered_map<std::string, const json::Value *> &getComponents() const;

    // minecraft:pushable_by_entity presets, parsed once per component change
    const PushableByEntityPresets &getPushPresets() const;

    std::vector<std::string> getFamilies() const;

    float getMovementSpeed() const;

    bool hasComponentGroup(const std::string &group) const;

    void addComponentGroup(const std::string &group);

    void removeComponentGroup(const std::string &group);

    void spawnPendingRiders(ServerNetworkHandler &owner) {
        mAddRider.spawnPending(owner, *this);
    }

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

    const std::string &getSpawnEvent() const {
        return mSpawnEvent;
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

    bool hasCarriedBlock() const {
        return mHasCarriedBlock;
    }

    void setCarriedBlock(ServerNetworkHandler &owner, const BlockState &state);

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
        return mOwnerId != NO_OWNER || !mLegacyOwnerName.empty() || mOwnerless;
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

    bool isPanicking() const {
        return mPanicking;
    }

    void setPanicking(bool panicking) {
        mPanicking = panicking;
    }

    bool isAvoidingMobs() const {
        return mAvoidingMobs;
    }

    void setAvoidingMobs(bool avoiding) {
        mAvoidingMobs = avoiding;
    }

    int32_t getInactivityTicks() const {
        return mInactivityTicks;
    }

    ServerPlayer *getOwner(ServerNetworkHandler &owner) const;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    int64_t getLastHurtTick() const {
        return mLastHurtTick;
    }

    // the death message key and cause of the last hurt, no actors kept
    const ActorDamageSource &getLastHurtSource() const {
        return mLastHurtSource;
    }

    void setLastHurtSource(const ActorDamageSource &source) {
        mLastHurtSource = ActorDamageSource();
        mLastHurtSource.mDeathMessageKey = source.mDeathMessageKey;
        mLastHurtSource.mCause = source.mCause;
    }

    uint64_t getLastHurtBy() const {
        return mLastHurtBy;
    }

    uint32_t getHurtCount() const {
        return mHurtCount;
    }

    bool setTarget(ServerNetworkHandler &owner, uint64_t runtimeId);

    void clearTarget();

    // viewers pose the mob from its target data (the skeleton's drawn bow); off where the data means something else
    virtual bool syncsTargetData() const {
        return true;
    }

    Actor *getTarget(ServerNetworkHandler &owner) const;

    bool canTarget(const Actor &actor) const;

    // minecraft:rideable with a free seat and an accepted family
    bool acceptsRider(const MobActor &rider) const;

    float distanceSquaredTo(const Actor &other) const;

    // degrees per tick, from the active minecraft:movement.* component
    float getMaxTurn() const;

    // from minecraft:jump.static; negative without it
    float getJumpPower() const;

    // no block on the eye-to-eye ray
    bool canSee(ServerNetworkHandler &owner, const Actor &other) const;

    static ServerPlayer *findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId);

    static Actor *findActor(ServerNetworkHandler &owner, uint64_t runtimeId);

    MobEquipment &getEquipment() {
        return mEquipment;
    }

    const MobEquipment &getEquipment() const {
        return mEquipment;
    }

    MobLeash &getLeash() {
        return mLeash;
    }

    MobAdmiration &getAdmiration() {
        return mAdmiration;
    }

    const MobAdmiration &getAdmiration() const {
        return mAdmiration;
    }

    MobDweller &getDweller() {
        return mDweller;
    }

    const MobDweller &getDweller() const {
        return mDweller;
    }

    MobAnger &getAnger() {
        return mAnger;
    }

    int getInventoryCapacity() const;

    // a tamed mount with a horse container_type: its saddle, armor and chest slots
    Container *getEntityContainer() override;

    std::vector<ItemStack> rollLoot(ServerNetworkHandler &owner, const std::string &path);

    void onKilledActor(ServerNetworkHandler &owner, Actor &victim);

    bool isCelebrating() const {
        return mCelebrationTicks > 0;
    }

    Vector3f getSeatOffset(size_t index, size_t passengerCount) const override;

    Vector3f getDismountPosition(size_t index, size_t passengerCount) const override;

    void onPassengerAdded(ServerNetworkHandler &owner, Actor &passenger) override;

    void onPassengerRemoved(ServerNetworkHandler &owner, Actor &passenger) override;

    bool isMountTaming() const;

    void attemptMountTame(ServerNetworkHandler &owner, ServerPlayer &rider);

    // the success of minecraft:tameable or minecraft:tamemount; false when untameable, tamed or cancelled
    bool tameTo(ServerNetworkHandler &owner, Actor &tamer, const char *component, bool showEffects);

    // tamed with no owner, as a script's EntityTameMountComponent.tame does (sub_143C9C590)
    bool tameWithoutOwner(ServerNetworkHandler &owner, const char *component, bool showEffects);

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
    // the minecraft:inventory slots of the mob's equipment, as a container the UI can show
    class InventoryView : public Container {
    public:
        explicit InventoryView(MobActor &mob) : mMob(mob) {}

        int getContainerSize() const override {
            return mMob.getInventoryCapacity();
        }

        const ItemStack &getContainerItem(int slot) const override {
            return mMob.mEquipment.getInventoryItem(slot);
        }

        void setContainerItem(int slot, ItemStack item) override {
            mMob.mEquipment.setInventoryItem(slot, std::move(item));
        }

    private:
        MobActor &mMob;
    };

    friend class RideControlSystem;
    friend class RideControlProbe;

    // read by every mob every tick: resolved once per component change, not by name
    struct SensorComponents {
        const json::Value *mEntitySensor = nullptr;
        const json::Value *mEnvironmentSensor = nullptr;
        const json::Value *mHurtOnCondition = nullptr;
        const json::Value *mTargetNearbySensor = nullptr;
    };

    // read by getPhysics() every tick: resolved once per component change
    struct DefinitionPhysics {
        bool mSwims = false;
        bool mClimbsLadders = false;
        bool mClimbsWalls = false;
        bool mDefinitionFriction = false;
        float mBounciness = 0.0f;
        float mFrictionModifier = 1.0f;
        float mAirDragModifier = 1.0f;
        bool mUniformAirDrag = false;
        bool mHasAutoStep = false;
        float mAutoStep = 0.0f;
        float mControlledAutoStep = 0.0f;
        BuoyancySettings mBuoyancy;
    };

    const DefinitionPhysics &_getDefinitionPhysics() const;

    void _rebuildComponents() const;

    const SensorComponents &_getSensorComponents() const;

    void _registerGoals(ServerNetworkHandler &owner);

    void _syncBody(ServerNetworkHandler &owner);

    void _syncTarget(ServerNetworkHandler &owner);

    void _markComponentsChanged();

    void _tickLifecycle(ServerNetworkHandler &owner);

    void _tickSensors(ServerNetworkHandler &owner);

    void _tickEntitySensor(ServerNetworkHandler &owner);

    void _tickTargetNearbySensor(ServerNetworkHandler &owner);

    void _tickHurtOnCondition(ServerNetworkHandler &owner);

    void _tickInactivity(ServerNetworkHandler &owner);

    int32_t _countSensedEntities(ServerNetworkHandler &owner, const json::Value &subsensor, bool playersOnly,
                                 bool relativeRange);

    void _tickTimer(ServerNetworkHandler &owner);

    void _tickAttackCooldown(ServerNetworkHandler &owner);

    int _experienceReward(const char *key, const ServerPlayer *player) const;

    int _deathExperience(const ServerPlayer *killer) const;

    void _tickCelebration(ServerNetworkHandler &owner);

    void _startCelebration(ServerNetworkHandler &owner, const json::Value &component);

    void _tickShaking(ServerNetworkHandler &owner);

    void _tickBreathing(ServerNetworkHandler &owner);

    void _tickFlopping(ServerNetworkHandler &owner);

    void _tickSpellEffects();

    void _tickTransformation(ServerNetworkHandler &owner);

    bool _senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source);

    bool _traceSight(ServerNetworkHandler &owner, const Actor &other) const;

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

    bool _tryNameTag(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryLeash(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

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

    void _givePlayerItem(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack item);

    void _emitInteractParticle(ServerNetworkHandler &owner, const ServerPlayer &player, const json::Value &particle,
                               const std::string &type);

    // true when an attribute the client must hear about changed
    bool _syncMovementAttributes();

    bool _syncHealthComponent();

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
    bool mOwnerless = false;
    int64_t mSyncedOwnerId = NO_OWNER;
    bool mSitting = false;
    bool mGoalsRegistered = false;
    bool mGoalsDirty = false;
    bool mBodyDirty = true;
    float mScale = 1.0f;
    bool mDefinitionStarted = false;
    bool mBorn = false;
    int32_t mActorVersion = 0;
    std::string mSpawnEvent;
    bool mEquipmentInherited = false;
    const json::Value *mSpellEffectsComponent = nullptr;
    const json::Value *mTimerComponent = nullptr;
    int32_t mTimerTicks = 0;
    const json::Value *mTransformationComponent = nullptr;
    int32_t mTransformationTicks = 0;
    bool mTransformed = false;
    bool mConvertsOnDeath = false;
    // the transformation the last hit's damage sensor added, until the end of the tick
    // the target, not a pointer: the component table is rebuilt when groups change
    std::string mHitTransformation;
    struct SightCache {
        int64_t mTick = -1;
        uint64_t mTargetId = 0;
        bool mVisible = false;
    };
    mutable SightCache mSightCache;
    mutable const json::Value *mSlowdownImmunityComponent = nullptr;
    mutable std::vector<std::string> mSlowdownImmuneBlocks;
    uint64_t mParentRuntimeId = 0;
    Vector3f mHomePosition;
    bool mHasHome = false;
    Vector3i mEventBlock;
    bool mHasEventBlock = false;
    BlockState mCarriedBlock;
    bool mHasCarriedBlock = false;
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
    mutable std::string mComponentKey;
    mutable SensorComponents mSensorComponents;
    mutable bool mSensorComponentsStale = true;
    int64_t mLastHurtTick = INT64_MIN / 2;
    uint64_t mLastHurtBy = 0;
    ActorDamageSource mLastHurtSource;
    uint32_t mHurtCount = 0;
    uint64_t mTargetRuntimeId = 0;
    uint64_t mSyncedTargetRuntimeId = 0;
    PathNavigation mNavigation;
    MoveControl mMoveControl;
    LookControl mLookControl;
    JumpControl mJumpControl;
    BodyControl mBodyControl;
    MobEquipment mEquipment;
    InventoryView mInventoryView{*this};
    MobEntitySpawner mEntitySpawner;
    std::vector<int32_t> mEntitySensorCooldowns;
    int32_t mTemper = 0;
    mutable float mRolledMovementSpeed = -1.0f;
    mutable float mRolledJumpStrength = -1.0f;
    RideControlState mRideControl;
    MobAnger mAnger;
    MobDweller mDweller;
    MobAddRider mAddRider;
    MobAdmiration mAdmiration;
    MobLeash mLeash;
    MobAreaAttack mAreaAttack;
    MobExplode mExplode;
    MobEquipmentChanged mEquipmentChanged;
    const json::Value *mAttackCooldownComponent = nullptr;
    int32_t mAttackCooldownTicks = 0;
    int32_t mCelebrationTicks = 0;
    int32_t mCelebrationSoundTicks = 0;
    static constexpr int32_t UNSET_AIR_SUPPLY = INT32_MIN;
    int32_t mAirSupply = UNSET_AIR_SUPPLY;
    bool mPanicking = false;
    bool mAvoidingMobs = false;
    int32_t mInactivityTicks = 0;
    bool mTargetInsideRange = false;
    bool mTargetOutsideRange = false;
    bool mTargetSeenInsideRange = false;
    const json::Value *mCelebrationComponent = nullptr;
    mutable DefinitionPhysics mDefinitionPhysics;
    mutable bool mDefinitionPhysicsStale = true;
    mutable PushableByEntityPresets mPushPresets;
};

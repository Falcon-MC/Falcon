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
#include "World/Actor/ActorLeash.h"
#include "World/Actor/Mob/Component/MobAnger.h"
#include "World/Actor/Mob/Component/MobAmbientSounds.h"
#include "World/Actor/Mob/Component/MobAngerLevel.h"
#include "World/Actor/Mob/Component/MobEffectAura.h"
#include "World/Actor/Mob/Component/MobTeleport.h"
#include "World/Actor/Mob/Component/MobDweller.h"
#include "World/Actor/Mob/Component/MobEconomy.h"
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
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Level;
class LootTable;
class ServerNetworkHandler;
class ServerPlayer;

class MobActor : public ServerActor {
public:
    // the share of the ageable duration one feed item takes off when its growth is left out: a feed_items entry
    // starts at 0.1
    static constexpr float DEFAULT_FEED_GROWTH = 0.1f;

    bool canFreeze() const override {
        return true;
    }

    // generated mobs only: the event their first tick fires, like villager_v2's minecraft:spawn_from_village
    static constexpr const char *TAG_SPAWN_EVENT = "SpawnEvent";

    MobActor(uint64_t runtimeId, const std::string &identifier);

    bool canStandOnPowderSnow() const override;

    float getArmorKnockbackResistance() const override;

    virtual ActorCategory getCategory() const = 0;

    virtual bool preventsSleep() const {
        return false;
    }

    // a zombie villager: its cure lowers the prices of the villager it becomes and of the villagers around
    virtual bool curesIntoTrader() const {
        return false;
    }

    // a villager reached by a nearby cure
    virtual bool isNearbyCureTarget() const {
        return false;
    }

    ActorSize getSize() const override = 0;

    // minecraft:collision_box of the active groups
    ActorSize definitionSize(const ActorSize &fallback) const;

    PhysicsComponent getPhysics() const override;

    // minecraft:mob_effect_immunity refuses the effects it lists
    bool canBeAffected(MobEffectId id) const override;

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

    // a player hit by the mob takes the difficulty's share; the game leaves the evoker's fangs out
    virtual bool scalesDamageWithDifficulty() const {
        return true;
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

    // a catalyst takes what the death would drop, worked out as if a player had made the kill
    int32_t takeDeathExperience(ServerNetworkHandler &owner) override;

    // now instead of on the first tick; at most once
    void startDefinition(ServerNetworkHandler &owner);

    void applyDefaults(Difficulty difficulty);

    // only a loaded actor runs its pending upgrades; any other cause takes the current version
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

    // spawnWithoutBehaviors stops every goal for good
    void removeBehaviors(ServerNetworkHandler &owner);

    bool hasBehaviorsRemoved() const {
        return mBehaviorsRemoved;
    }

    // the running goals stop; they may start again on the next tick
    void stopGoals(ServerNetworkHandler &owner) {
        mGoalSelector.stopAll(owner, *this);
    }

    MobExplode &getExplode() {
        return mExplode;
    }

    PathNavigation &getNavigation() {
        return mNavigation;
    }

    const PathNavigation &getNavigation() const {
        return mNavigation;
    }

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    bool dodgeProjectile(ServerNetworkHandler &owner, const ServerActor &projectile) override {
        return mTeleport.dodge(owner, *this, projectile);
    }

    // one try at a spot of minecraft:teleport's random_teleport_cube; false without the component
    bool randomTeleport(ServerNetworkHandler &owner) {
        return mTeleport.randomTeleport(owner, *this);
    }

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    void causeFallDamage(ServerNetworkHandler &owner, float distance) override;

    float absorbDamage(float amount, const ActorDamageSource &source) const override {
        return mEquipment.absorbDamage(amount, source);
    }

    void hurtArmor(ServerNetworkHandler &owner, float amount, const ActorDamageSource &source) override {
        mEquipment.hurtArmor(owner, *this, amount, source);
    }

    bool ignoresMovementSlowdown(const std::string &blockName) const override;

    bool burnsInDaylight() const override;

    bool shieldFromSunlight() const override;

    // the same on every difficulty: a player takes the difficulty's share when it is hurt
    virtual float getAttackDamage() const;

    const json::Value *getDefinition() const;

    const json::Value *getComponent(const std::string &name) const;

    // found by definition component id: no std::string is built for the literal
    const json::Value *getComponent(const char *name) const;

    // minecraft:persistent in the active groups keeps the mob too
    bool isPersistent() const override;

    const std::unordered_map<std::string, const json::Value *> &getComponents() const;

    // minecraft:pushable_by_entity presets, parsed once per component change
    const PushableByEntityPresets &getPushPresets() const;

    std::vector<std::string> getFamilies() const;

    // the game overrides the movement attribute of a few mobs
    virtual float getMovementSpeed() const;

    // added to the movement attribute while a goal holds it, as drinking a potion does: the ratio its moves keep
    float getMovementModifierScale() const;

    void setMovementModifier(float modifier) {
        mMovementModifier = modifier;
    }

    // how far its targets are kept: minecraft:follow_range, 16 without it; the game sets a few mobs' in code
    virtual float getFollowRange() const;

    bool hasComponentGroup(const std::string &group) const;

    void addComponentGroup(const std::string &group);

    void removeComponentGroup(const std::string &group);

    void spawnPendingRiders(ServerNetworkHandler &owner) {
        mAddRider.spawnPending(owner, *this);
    }

    void fireEvent(ServerNetworkHandler &owner, const std::string &event, Actor *other = nullptr);

    // a shield stopped this mob's melee hit: minecraft:ravager_blocked pushes the blocker off and reacts
    void onAttackBlocked(ServerNetworkHandler &owner, Actor &blocker);

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

    // a mob's sleeping state lives only in its flags
    void setSleepingFlag(ServerNetworkHandler &owner, bool sleeping) {
        _setFlag(owner, ActorFlag::Sleeping, sleeping);
    }

    void setWantsToWake(ServerNetworkHandler &owner, bool wants) {
        _setFlag(owner, ActorFlag::WantsToWake, wants);
    }

    bool isInLove() const {
        return mLoveTicks > 0;
    }

    void finishBreeding(ServerNetworkHandler &owner);

    int32_t getVariant() const;

    // air in ticks; a mob that never breathed yet holds its full supply
    int32_t getAirSupply() const;

    void setAirSupply(int32_t ticks);

    int32_t getMaxAirSupply() const;

    // the drowning damage hits once the air falls to this
    static constexpr int32_t MIN_AIR_SUPPLY = -20;

    void setVariant(int32_t variant);

    void inheritVariant(const MobActor &firstParent, const MobActor &secondParent);

    // minecraft:color or minecraft:is_dyeable say whether the mob has a color: the value is the one its last color
    // group or dye gave
    bool hasColor() const;

    int32_t getColor() const;

    void setColor(ServerNetworkHandler &owner, int32_t color);

    // minecraft:offspring combine_parent_colors, once the baby's born event gave it a color of its own
    void inheritColor(ServerNetworkHandler &owner, const MobActor &firstParent, const MobActor &secondParent);

    // minecraft:breedable inherit_tamed, before the baby's definition starts: its born event tests is_tamed
    void inheritOwner(ServerNetworkHandler &owner, const MobActor &firstParent, const MobActor &secondParent);

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

    // a bell rang close by: it hides until its hide goal is done with it
    bool heardBell() const {
        return mHeardBell;
    }

    void setHeardBell(bool heard) {
        mHeardBell = heard;
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

    // the runtime id of the last actor this mob damaged
    uint64_t getLastHurtMob() const {
        return mLastHurtMob;
    }

    void setLastHurtMob(uint64_t runtimeId) {
        mLastHurtMob = runtimeId;
    }

    // unique id of the leader minecraft:behavior.follow_target_leader follows, 0 for none
    int64_t getTargetCaptainId() const {
        return mTargetCaptainId;
    }

    void setTargetCaptainId(int64_t uniqueId) {
        mTargetCaptainId = uniqueId;
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

    // minecraft:rideable with a free seat and, when checked, an accepted family
    bool acceptsRider(const MobActor &rider, bool checkFamilies = true) const;

    float distanceSquaredTo(const Actor &other) const;

    // degrees per tick, from the active minecraft:movement.* component
    float getMaxTurn() const;

    // from minecraft:jump.static; negative without it
    float getJumpPower() const;

    // no block on the eye-to-eye ray
    bool canSee(ServerNetworkHandler &owner, const Actor &other) const;

    static ServerPlayer *findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId);

    static Actor *findActor(ServerNetworkHandler &owner, uint64_t runtimeId);

    void useSpawnEquipment(ServerNetworkHandler &owner, const std::string &table, float dropChance);

    MobEquipment &getEquipment() {
        return mEquipment;
    }

    const MobEquipment &getEquipment() const {
        return mEquipment;
    }

    ActorLeash &getLeash() {
        return mLeash;
    }

    ActorLeash *getActorLeash() override {
        return &mLeash;
    }

    const json::Value *getLeashableComponent() const override;

    const json::Value *getLeashableToComponent() const override;

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

    MobEconomy &getEconomy() {
        return mEconomy;
    }

    const MobEconomy &getEconomy() const {
        return mEconomy;
    }

    MobAnger &getAnger() {
        return mAnger;
    }

    MobAngerLevel &getAngerLevel() {
        return mAngerLevel;
    }

    const MobAngerLevel &getAngerLevel() const {
        return mAngerLevel;
    }

    const MobAmbientSounds &getAmbientSounds() const {
        return mAmbientSounds;
    }

    // minecraft:suspect_tracking: the block something last disturbed the mob from, and the tick it did
    const std::optional<Vector3i> &getSuspectPosition() const {
        return mSuspectPosition;
    }

    int64_t getSuspectTick() const {
        return mSuspectTick;
    }

    void setSuspectPosition(const Vector3i &position, int64_t tick) {
        mSuspectPosition = position;
        mSuspectTick = tick;
    }

    void clearSuspectPosition() {
        mSuspectPosition.reset();
    }

    // 0 without minecraft:inventory
    int getInventoryCapacity() const;
    int getInventoryStrength() const;

    // a tamed mount with a horse container_type: its saddle, armor and chest slots
    Container *getEntityContainer() override;

    // the minecraft:inventory slots, the equippable body slot read from the worn body armor
    Container &getInventoryContainer();

    // the items of the equippable slots, by slot number, to compare after a window change
    std::vector<ItemStack> getEquippableItems() const;

    // runs on_equip and on_unequip of the slots that were filled or emptied since before
    void onEquippableItemsChanged(ServerNetworkHandler &owner, const std::vector<ItemStack> &before, Actor *changer);

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

    // tamed with no owner, as a script's EntityTameMountComponent.tame does
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

    // a natural spawn the spawner settled on, before the mob is made: a few mobs move it, or call it off
    virtual bool placeNaturalSpawn(ServerNetworkHandler &owner, Level &level, Vector3f &position,
                                   std::mt19937 &random) const {
        (void) owner;
        (void) level;
        (void) position;
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

    // false skips the goals and the navigation of the tick
    virtual bool runsGoals() const {
        return true;
    }

    // false leaves the loot and the experience to dropDeathRewards, called later by the mob
    virtual bool dropsRewardsOnDeath() const {
        return true;
    }

    // loot, equipment and experience, under domobloot; killer may be null
    void dropDeathRewards(ServerNetworkHandler &owner, ServerPlayer *killer, int32_t lootingLevel);

    void tickControls(ServerNetworkHandler &owner);

private:
    // the minecraft:inventory slots of the mob's equipment, as a container the UI can show
    class InventoryView : public Container {
    public:
        explicit InventoryView(MobActor &mob) : mMob(mob) {}

        int getContainerSize() const override {
            if (mMob._isMountInventory() && mMob.getComponent("minecraft:is_chested") == nullptr)
                return getEquipmentSlotCount();
            return mMob.getInventoryCapacity();
        }

        int getEquipmentSlotCount() const override {
            return mMob._isMountInventory() ? mMob._equippableSlotCount() : 0;
        }

        const ItemStack &getContainerItem(int slot) const override {
            return mMob._equippableItem(slot);
        }

        void setContainerItem(int slot, ItemStack item) override {
            mMob._setEquippableItem(slot, std::move(item));
        }

        bool canHoldInSlot(int slot, const ItemStack &item) const override {
            return slot >= 0 && slot < getContainerSize()
                   && (slot >= getEquipmentSlotCount() || item.isAir() || item.mCount == 1)
                   && mMob._acceptsInEquippableSlot(slot, item);
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
        bool mHasCollision = true;
        bool mSwims = false;
        bool mClimbsLadders = false;
        bool mClimbsWalls = false;
        bool mBlockClimber = false;
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

    const json::Value *_findComponent(std::string_view name) const;

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

    void _tickBreakBlocks(ServerNetworkHandler &owner);
    void _tickDamageOverTime(ServerNetworkHandler &owner);

    void _tickScheduler(ServerNetworkHandler &owner);

    void _tickAttackCooldown(ServerNetworkHandler &owner);

    int _experienceReward(const char *key, const ServerPlayer *player, bool lastHitByPlayer = false) const;

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

    void _passTradesTo(ServerNetworkHandler &owner, MobActor &into, bool keepLevel);

    void _fireComponentEvent(ServerNetworkHandler &owner, const char *component);

    void _setFlag(ServerNetworkHandler &owner, ActorFlag flag, bool value);

    bool _tryTame(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    // the feeding steps set used when they take the held item: onInteract uses it up once for all of them
    bool _tryFeedBaby(ServerNetworkHandler &owner, const ItemStack &held, bool &used);

    bool _trySpawnEggBaby(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryStartLove(ServerNetworkHandler &owner, const ItemStack &held, bool &used);

    bool _trySit(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryDye(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryHeal(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held, bool &used);

    int32_t _mixColors(int32_t first, int32_t second);

    bool _tryInteract(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryNameTag(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryLeash(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryTieLeads(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryFeedMount(ServerNetworkHandler &owner, const ItemStack &held, bool &used);

    bool _tryMount(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryTrade(ServerNetworkHandler &owner, ServerPlayer &player);

    const json::Value *_seatFor(size_t index, size_t passengerCount) const;

    float _rolledValue(const json::Value *component, float &rolled, float fallback) const;

    // by place in the definition's list: the game keeps the equippable items at the head of the inventory in
    // that order, whatever "slot" says (the llama's only slot is numbered 1 and sits first)
    const json::Value *_equippableSlot(int32_t index) const;

    // "slot" 1 is the body armor, the one worn and shown
    bool _isBodySlot(int32_t index) const;

    // the body slot when the equippable slot is the body one, else the minecraft:inventory slot
    const ItemStack &_equippableItem(int32_t index) const;

    void _setEquippableItem(int32_t index, ItemStack item);

    bool _acceptsInEquippableSlot(int32_t index, const ItemStack &item) const;

    bool _equipFromHand(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &slotName);

    void _dropEquipmentSlot(ServerNetworkHandler &owner, const std::string &slotName, float yOffset);

    void _spawnLoot(ServerNetworkHandler &owner, Level &level, const std::string &path);

    void _givePlayerItem(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack item);

    void _emitInteractParticle(ServerNetworkHandler &owner, const ServerPlayer &player, const json::Value &particle);

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
    std::optional<int32_t> mColor;
    bool mHeardBell = false;
    bool mGoalsRegistered = false;
    bool mGoalsDirty = false;
    bool mBehaviorsRemoved = false;
    bool mBodyDirty = true;
    float mScale = 1.0f;
    bool mDefinitionStarted = false;
    bool mBorn = false;
    int32_t mActorVersion = 0;
    std::string mSpawnEvent;
    // the mob a transformation replaces, until the new one's first event ran
    MobActor *mTransformedFrom = nullptr;
    bool mEquipmentInherited = false;
    const json::Value *mSpellEffectsComponent = nullptr;
    const json::Value *mTimerComponent = nullptr;
    // the world tick the timer runs out on, as saved; a loaded timer resumes from it
    int64_t mTimerStamp = 0;
    bool mTimerExecuted = false;
    // a saved stamp waiting for the timer component to come back after a load
    bool mTimerLoaded = false;
    const json::Value *mBreakBlocksComponent = nullptr;
    std::unordered_set<std::string> mBreakableBlocks;
    const json::Value *mDamageOverTimeComponent = nullptr;
    int32_t mHurtCountdown = 0;
    const json::Value *mSchedulerComponent = nullptr;
    // the scheduled event whose filters held last; -1 for none
    int32_t mScheduledEvent = -1;
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
    // mComponents by definition component id, filled as names are looked up; emptied with the components: a group
    // added or removed changes what a name finds
    mutable std::vector<const json::Value *> mRecentComponents;
    mutable SensorComponents mSensorComponents;
    mutable bool mSensorComponentsStale = true;
    int64_t mLastHurtTick = INT64_MIN / 2;
    uint64_t mLastHurtBy = 0;
    uint64_t mLastHurtMob = 0;
    int64_t mTargetCaptainId = 0;
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
    MobEconomy mEconomy;
    bool _isMountInventory() const;
    int _equippableSlotCount() const;
    int mLoadedInventoryStrength = -1;
    InventoryView mInventoryView{*this};
    MobEntitySpawner mEntitySpawner;
    std::vector<int32_t> mEntitySensorCooldowns;
    int32_t mTemper = 0;
    mutable float mRolledMovementSpeed = -1.0f;
    float mMovementModifier = 0.0f;
    mutable float mRolledJumpStrength = -1.0f;
    RideControlState mRideControl;
    MobAnger mAnger;
    MobAngerLevel mAngerLevel;
    MobAmbientSounds mAmbientSounds;
    MobEffectAura mEffectAura;
    MobTeleport mTeleport;
    std::optional<Vector3i> mSuspectPosition;
    int64_t mSuspectTick = 0;
    MobDweller mDweller;
    MobAddRider mAddRider;
    MobAdmiration mAdmiration;
    ActorLeash mLeash;
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
    bool mDeathExperienceTaken = false;
};

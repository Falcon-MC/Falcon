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
#include "Actor/Mob/MobEquipment.h"
#include "Actor/ServerActor.h"
#include "Core/Json/Json.h"
#include "Server/PropertiesSettings.h"

#include <cstdint>
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

    ActorSize getSize() const override = 0;

    virtual float getDefaultMaxHealth() const = 0;

    virtual int getExperienceDrop() const { return 0; }

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

    float absorbDamage(float amount, const DamageSource &source) const override {
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

    void fireEvent(ServerNetworkHandler &owner, const std::string &event);

    void markBorn();

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    bool isInLove() const {
        return mLoveTicks > 0;
    }

    void finishBreeding(ServerNetworkHandler &owner);

    bool isTamed() const {
        return !mTamedBy.empty();
    }

    const std::string &getTamedBy() const {
        return mTamedBy;
    }

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

    void setTarget(uint64_t runtimeId);

    void clearTarget();

    Actor *getTarget(ServerNetworkHandler &owner) const;

    bool canTarget(const Actor &actor) const;

    float distanceSquaredTo(const Actor &other) const;

    static ServerPlayer *findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId);

    static Actor *findActor(ServerNetworkHandler &owner, uint64_t runtimeId);

    MobEquipment &getEquipment() {
        return mEquipment;
    }

protected:
    virtual void registerGoals(GoalSelector &goalSelector) {
        (void) goalSelector;
    }

    void tickControls(ServerNetworkHandler &owner);

private:
    void _rebuildComponents() const;

    void _registerGoals(ServerNetworkHandler &owner);

    void _syncBody(ServerNetworkHandler &owner);

    void _markComponentsChanged();

    void _tickLifecycle(ServerNetworkHandler &owner);

    void _tickSensors(ServerNetworkHandler &owner);

    void _fireComponentEvent(ServerNetworkHandler &owner, const char *component);

    void _setFlag(ServerNetworkHandler &owner, ActorFlag flag, bool value);

    bool _tryTame(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryFeedBaby(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _tryStartLove(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held);

    bool _trySit(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _tryInteract(ServerNetworkHandler &owner, ServerPlayer &player);

    void _spawnLoot(ServerNetworkHandler &owner, Level &level, const std::string &path);

    void _appendDefinitionData(EntityDataMap &metadata) const;

    void _syncOwner(ServerNetworkHandler &owner);

    GoalSelector mGoalSelector;
    bool mTargetAcquired = false;
    bool mTargetEscaped = false;
    int32_t mLoveTicks = 0;
    int32_t mBreedCooldown = 0;
    int32_t mInteractCooldown = 0;
    int32_t mAgeTicks = 0;
    std::string mTamedBy;
    uint64_t mOwnerRuntimeId = 0;
    bool mSitting = false;
    bool mGoalsRegistered = false;
    bool mGoalsDirty = false;
    bool mBodyDirty = true;
    float mScale = 1.0f;
    bool mDefinitionStarted = false;
    bool mBorn = false;
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
};

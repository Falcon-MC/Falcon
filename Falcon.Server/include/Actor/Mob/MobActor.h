#pragma once

#include "Actor/AI/Control/BodyControl.h"
#include "Actor/AI/Control/JumpControl.h"
#include "Actor/AI/Control/LookControl.h"
#include "Actor/AI/Control/MoveControl.h"
#include "Actor/AI/Goal/GoalSelector.h"
#include "Actor/AI/Navigation/PathNavigation.h"
#include "Actor/ActorCategory.h"
#include "Actor/ActorSize.h"
#include "Actor/ServerActor.h"
#include "Server/PropertiesSettings.h"

#include <cstdint>
#include <string>

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

    virtual float getAttackDamage(Difficulty difficulty) const {
        (void) difficulty;
        return 0.0f;
    }

    int64_t getLastHurtTick() const {
        return mLastHurtTick;
    }

    uint64_t getLastHurtBy() const {
        return mLastHurtBy;
    }

    uint32_t getHurtCount() const {
        return mHurtCount;
    }

    void setTarget(uint64_t runtimeId) {
        mTargetRuntimeId = runtimeId;
    }

    void clearTarget() {
        mTargetRuntimeId = 0;
    }

    ServerPlayer *getTarget(ServerNetworkHandler &owner) const;

    bool canTarget(const ServerPlayer &player) const;

    float distanceSquaredTo(const Actor &other) const;

    static ServerPlayer *findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId);

protected:
    virtual void registerGoals(GoalSelector &goalSelector) {
        (void) goalSelector;
    }

    void tickControls(ServerNetworkHandler &owner);

private:
    GoalSelector mGoalSelector;
    bool mGoalsRegistered = false;
    int64_t mLastHurtTick = INT64_MIN / 2;
    uint64_t mLastHurtBy = 0;
    uint32_t mHurtCount = 0;
    uint64_t mTargetRuntimeId = 0;
    PathNavigation mNavigation;
    MoveControl mMoveControl;
    LookControl mLookControl;
    JumpControl mJumpControl;
    BodyControl mBodyControl;
};

#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3i.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class Actor;

// minecraft:behavior.emerge: climbs out of the ground, then waits out its cooldown
class EmergeGoal final : public Goal {
public:
    struct Settings {
        int64_t mDurationTicks = 0;
        int64_t mCooldownTicks = 0;
        std::shared_ptr<json::Value> mOnDone;
    };

    explicit EmergeGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    int64_t mNextUseTick = 0;
    int64_t mEndTick = 0;
};

// minecraft:behavior.dig: left alone long enough, the mob burrows and is gone
class DigGoal final : public Goal {
public:
    struct Settings {
        int64_t mDurationTicks = 0;
        int64_t mIdleTicks = 0;
        bool mAllowDigWhenNamed = false;
        bool mDigsInDaylight = false;
        bool mVibrationIsDisturbance = false;
        bool mSuspicionIsDisturbance = false;
        std::shared_ptr<json::Value> mOnStart;
    };

    explicit DigGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    // the idle time counts from when the goal was made
    int64_t mCreatedTick = -1;
    int64_t mEndTick = 0;
};

// minecraft:behavior.roar: roars at the first entity it is angry at, then takes it as its target
class RoarGoal final : public Goal {
public:
    explicit RoarGoal(int64_t durationTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int64_t mDurationTicks;
    int64_t mEndTick = 0;
    int64_t mTargetId = -1;
};

// minecraft:behavior.sniff: smells for the nearest nuisance, suspects where it stands and grows angry if it is close
class SniffGoal final : public Goal {
public:
    struct Settings {
        int64_t mDurationTicks = 0;
        float mSniffingRadius = 0.0f;
        float mSuspicionRadiusHorizontal = 0.0f;
        float mSuspicionRadiusVertical = 0.0f;
        float mCooldownMin = 0.0f;
        float mCooldownMax = 0.0f;
    };

    explicit SniffGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    // players first, then other mobs; the closest of the first kind found
    Actor *findCandidate(ServerNetworkHandler &owner, MobActor &mob) const;

private:
    Settings mSettings;
    int64_t mNextUseTick = 0;
    int64_t mEndTick = 0;
};

// minecraft:behavior.investigate_suspicious_location: walks to the suspect block
class InvestigateSuspiciousLocationGoal final : public Goal {
public:
    InvestigateSuspiciousLocationGoal(float speed, float goalRadius);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    std::optional<Vector3i> _suspect(const MobActor &mob) const;

    float mSpeed;
    float mGoalRadiusSquared;
    Vector3i mLocation;
    bool mFailed = false;
    bool mReached = false;
    bool mRepathed = false;
};

// minecraft:behavior.sonic_boom: charges, then hits its target through anything in a straight line
class SonicBoomGoal final : public Goal {
public:
    struct Settings {
        int64_t mDurationTicks = 0;
        int64_t mAttackDelayTicks = 0;
        int64_t mCooldownTicks = 0;
        float mSpeed = 0.0f;
        float mDamage = 0.0f;
        float mRangeHorizontal = 0.0f;
        float mRangeVertical = 0.0f;
        float mKnockbackHorizontal = 0.0f;
        float mKnockbackVertical = 0.0f;
        float mKnockbackHeightCap = 0.0f;
        std::string mAttackSound;
        std::string mChargeSound;
    };

    explicit SonicBoomGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    void _boom(ServerNetworkHandler &owner, MobActor &mob, Actor &target);

    Settings mSettings;
    std::optional<int64_t> mEndTick;
    std::optional<int64_t> mAttackTick;
    std::optional<int64_t> mCooldownEndTick;
};

class WardenGoals {
public:
    // null for a behavior that is not one of these
    static std::unique_ptr<Goal> create(const MobActor &mob, const std::string &behavior, const json::Value &component);
};

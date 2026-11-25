#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>

class WitherActor;

// minecraft:behavior.wither_random_attack_pos_goal: once the wither arms a dive above half health, it flies to a
// spot past its target, away from a random point around itself
class WitherRandomAttackPosGoal final : public Goal {
public:
    WitherRandomAttackPosGoal(float speed, int32_t horizontalRange, int32_t verticalRange);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mSpeed;
    int32_t mHorizontalRange;
    int32_t mVerticalRange;
    bool mRunning = false;
    Vector3f mWanted;
    std::mt19937 mRandom;
};

// minecraft:behavior.wither_target_highest_damage: targets the player who dealt the wither the most damage
class WitherTargetHighestDamageGoal final : public Goal {
public:
    WitherTargetHighestDamageGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    uint64_t mTargetRuntimeId = 0;
};

class WitherGoals {
public:
    // nullptr for another behavior or mob
    static std::unique_ptr<Goal> create(const MobActor &mob, const std::string &behavior, const json::Value &component);
};

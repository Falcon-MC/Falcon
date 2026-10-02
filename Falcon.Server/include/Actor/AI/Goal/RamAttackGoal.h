#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <string>

class Actor;

class RamAttackGoal : public Goal {
public:
    struct Settings {
        float mRunSpeed = 0.0f;
        float mRamSpeed = 0.0f;
        float mMinRamDistance = 0.0f;
        float mRamDistance = 0.0f;
        float mKnockbackForce = 0.0f;
        float mKnockbackHeight = 0.0f;
        int32_t mMinCooldownTicks = 0;
        int32_t mMaxCooldownTicks = 0;
        std::string mPreRamSound;
        std::string mRamImpactSound;
        std::shared_ptr<json::Value> mOnStart;
    };

    explicit RamAttackGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    enum class Phase {
        Approach,
        Prepare,
        Charge
    };

    void _tickApproach(ServerNetworkHandler &owner, MobActor &mob, const Actor &target);

    void _tickPrepare(MobActor &mob, const Actor &target);

    void _tickCharge(ServerNetworkHandler &owner, MobActor &mob, Actor &target);

    Settings mSettings;
    Phase mPhase = Phase::Approach;
    Vector3f mRamDirection;
    int64_t mNextRamTick = 0;
    int32_t mPhaseTicks = 0;
    bool mDone = false;
};

#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>

class Village;

/**
 * `minecraft:behavior.move_to_village`: walks to a random point of the mob's village, the one it dwells in or else the
 * closest one, while the mob is outside the village box. Long trips go in legs of three quarters of the follow range.
 */
class MoveToVillageGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int64_t mCooldownTicks = 0;
        float mGoalRadius = 0.0f;
        float mFollowRange = 0.0f;
    };

    explicit MoveToVillageGoal(const Settings &settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    std::shared_ptr<Village> _findVillage(ServerNetworkHandler &owner, const MobActor &mob) const;

    Vector3i _selectTarget(ServerNetworkHandler &owner, const MobActor &mob) const;

    Vector3f _targetPosition() const;

    void _moveCloser(ServerNetworkHandler &owner, MobActor &mob) const;

    void _checkRepeatedPath(const MobActor &mob);

    Settings mSettings;
    float mGoalRadiusSquared;
    Vector3f mTarget;
    int64_t mCooldown = 0;
    int32_t mStartDelay = 0;
    int32_t mTravelTicks = 0;
    int32_t mTicksAtTarget = 0;
    bool mEnteredVillage = false;
    bool mPathCheckPending = false;
    Vector3i mLastPathStart;
};

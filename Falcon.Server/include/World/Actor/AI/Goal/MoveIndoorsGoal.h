#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

// minecraft:behavior.move_indoors and move_outdoors: a villager caught under the open sky in rain or a raid walks
// to its bed; one under a roof once the celebration starts walks out under the sky
class MoveIndoorsGoal final : public Goal {
public:
    enum class Direction { Indoors, Outdoors };

    MoveIndoorsGoal(Direction direction, float speed, int32_t timeoutCooldownTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isUnderSky(ServerNetworkHandler &owner, const MobActor &mob) const;

    bool _wantsToMove(ServerNetworkHandler &owner, const MobActor &mob) const;

    Direction mDirection;
    float mSpeed;
    int32_t mTimeoutCooldownTicks;
    int32_t mCooldown = 0;
    Vector3f mTarget;
};

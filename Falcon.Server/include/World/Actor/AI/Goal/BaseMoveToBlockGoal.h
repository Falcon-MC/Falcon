#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Level;

// the game's walk to a block a subclass picks, the base of the farm and bookshelf goals: a search, a walk that
// gives up after 1200 ticks or when stuck, and a stay at the block
class BaseMoveToBlockGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int32_t mSearchRange = 0;
        int32_t mSearchHeight = 0;
        // random ground positions tried; 0 scans the box ring by ring from the mob
        int32_t mSearchCount = 0;
        float mGoalRadius = 0.0f;
        // a stuck walk ends the goal and holds it off for these ticks; 0 only repaths
        int32_t mGiveUpTicks = 0;
    };

    explicit BaseMoveToBlockGoal(Settings settings);

    // a search every 200 to 399 calls, while no stuck walk holds the goal off
    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    virtual bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) = 0;

    bool findTarget(ServerNetworkHandler &owner, MobActor &mob);

    bool isReached() const {
        return mReached;
    }

    const Vector3i &getTarget() const {
        return mTarget;
    }

    void setTarget(const Vector3i &target) {
        mTarget = target;
    }

    // the point walked to: the block above the target, at its low corner
    Vector3f targetPoint() const;

    void moveToTarget(MobActor &mob);

private:
    bool _canReach(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) const;

    bool _tryCandidate(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &origin, const Vector3i &position,
                       float &best);

    Settings mSettings;
    Vector3i mTarget;
    Vector3i mLastPathStart;
    int32_t mTravelTicks = 0;
    int32_t mStayTicks = 0;
    int32_t mGiveUpLeft = 0;
    int32_t mSearchCooldown = 0;
    bool mReached = false;
};

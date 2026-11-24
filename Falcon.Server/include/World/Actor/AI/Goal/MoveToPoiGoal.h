#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "World/Village/PoiBlueprint.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>

struct PoiInstance;

// walks a dweller to the point of interest of one type its village gave it; the shared base of the work and
// sleep goals
class MoveToPoiGoal : public Goal {
public:
    MoveToPoiGoal(float speed, PoiType type);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    static std::shared_ptr<PoiInstance> findClaimedPoi(const MobActor &mob, PoiType type);

protected:
    bool isReached() const {
        return mReached;
    }

    // the point's block
    const Vector3i &getTargetBlock() const {
        return mTargetBlock;
    }

    void moveToTarget(MobActor &mob);

private:
    bool _isReached(const MobActor &mob) const;

    float mSpeed;
    PoiType mType;
    int32_t mGiveUpTicks = 0;
    int32_t mIntervalTicks = 0;
    int32_t mTravelTicks = 0;
    int32_t mStayTicks = 0;
    bool mReached = false;
    bool mUseBoundingBox = false;
    float mRadiusSquared = 0.0f;
    Vector3i mTargetBlock;
    Vector3f mTarget;
    AxisAlignedBB mTargetBox;
    Vector3i mLastPathStart;
};

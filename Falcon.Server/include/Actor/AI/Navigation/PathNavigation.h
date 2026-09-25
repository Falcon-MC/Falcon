#pragma once

#include "Actor/AI/Navigation/Path.h"
#include "Actor/AI/Navigation/PathOptions.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

class PathNavigation {
public:
    void moveTo(const Vector3f &target, float speed);

    void stop(MobActor &mob);

    bool isDone() const {
        return !mHasTarget;
    }

    void setCanOpenDoors(bool canOpenDoors) {
        mOptions.mCanOpenDoors = canOpenDoors;
    }

    void setAvoidSun(bool avoidSun) {
        mOptions.mAvoidSun = avoidSun;
    }

    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    void _checkStuck(MobActor &mob);

    Path mPath;
    PathOptions mOptions;
    Vector3f mTarget;
    Vector3f mLastPosition;
    float mSpeed = 0.0f;
    bool mHasTarget = false;
    bool mNeedsPath = false;
    int32_t mStuckTicks = 0;
    int32_t mRepaths = 0;
};

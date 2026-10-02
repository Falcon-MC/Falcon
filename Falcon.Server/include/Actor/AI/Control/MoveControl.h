#pragma once

#include "Core/Math/Vector3f.h"

class JumpControl;
class MobActor;
class ServerNetworkHandler;

class MoveControl {
public:
    void setWantedPosition(const Vector3f &position, float speed);

    void stop();

    bool hasWanted() const {
        return mHasWanted;
    }

    const Vector3f &getWantedPosition() const {
        return mWantedPosition;
    }

    bool isMoving() const {
        return mMoving;
    }

    void setFlying(bool flying) {
        mFlying = flying;
    }

    void tick(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl);

private:
    void _tryJump(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl, float dx, float dz) const;

    void _tickFlying(MobActor &mob);

    void _tickSwimming(MobActor &mob);

    void _steerTowards(MobActor &mob, float cruise);

    Vector3f mWantedPosition;
    float mSpeed = 0.0f;
    bool mHasWanted = false;
    bool mMoving = false;
    bool mFlying = false;
};

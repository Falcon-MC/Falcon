#pragma once

#include "Core/Math/Vector3f.h"

class JumpControl;
class MobActor;
class ServerNetworkHandler;

class MoveControl {
public:
    void setWantedPosition(const Vector3f &position, float speedModifier);

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

    void tick(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl);

private:
    void _tryJump(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl, float dx, float dz) const;

    Vector3f mWantedPosition;
    float mSpeedModifier = 1.0f;
    bool mHasWanted = false;
    bool mMoving = false;
};

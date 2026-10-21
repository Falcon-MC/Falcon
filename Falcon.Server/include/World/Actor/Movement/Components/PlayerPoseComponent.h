#pragma once

#include "World/Actor/Movement/MovementConstants.h"

struct PlayerPoseComponent {
    bool mSneaking = false;
    bool mCrawling = false;
    float mHeight = MovementPhysics::PLAYER_HEIGHT;
};

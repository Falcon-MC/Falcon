#pragma once

struct PhysicsComponent {
    static constexpr float DEFAULT_GRAVITY = 0.08f;

    static constexpr float LIVING_STEP_HEIGHT = 0.5625f;

    bool mHasGravity = true;
    bool mHasCollision = true;
    bool mPushable = true;
    bool mFloatsInLiquid = true;
    bool mSwims = false;
    bool mClimbsLadders = false;
    bool mClimbsWalls = false;
    // mobs move with vertical speed first, gravity and drag after: a 0.42 jump climbs a full block;
    // other actors apply both before moving
    bool mGravityAfterMove = false;
    float mGravity = DEFAULT_GRAVITY;
    float mStepHeight = LIVING_STEP_HEIGHT;
};

#pragma once

struct PhysicsComponent {
    static constexpr float DEFAULT_GRAVITY = 0.08f;

    static constexpr float LIVING_STEP_HEIGHT = 0.5625f;

    bool mHasGravity = true;
    bool mHasCollision = true;
    bool mPushable = true;
    bool mFloatsInLiquid = true;
    float mGravity = DEFAULT_GRAVITY;
    float mStepHeight = LIVING_STEP_HEIGHT;
};

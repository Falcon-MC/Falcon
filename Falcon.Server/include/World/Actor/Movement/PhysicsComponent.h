#pragma once

#include "World/Actor/Movement/BuoyancySettings.h"

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
    // the minecraft:bounciness, friction_modifier and air_drag_modifier attributes: an actor declaring one moves
    // with the game's MobMovementFriction factors after the move
    bool mDefinitionFriction = false;
    float mBounciness = 0.0f;
    float mFrictionModifier = 1.0f;
    float mAirDragModifier = 1.0f;
    bool mUniformAirDrag = false;
    BuoyancySettings mBuoyancy;
};

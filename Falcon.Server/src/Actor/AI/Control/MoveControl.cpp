#include "Actor/AI/Control/MoveControl.h"

#include "Actor/AI/Control/JumpControl.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float PRECISION = 1.0e-5f;
    const float SPEED_FACTOR = 0.33f;
    const float SATURATED_SPEED_RATIO = 0.4756f;
    const float MIN_JUMP_HEIGHT = 0.01f;
    const float MAX_JUMP_HEIGHT = 1.1f;
    const char *MOVEMENT_ATTRIBUTE = "minecraft:movement";
}

void MoveControl::setWantedPosition(const Vector3f &position, float speedModifier) {
    mWantedPosition = position;
    mSpeedModifier = speedModifier;
    mHasWanted = true;
}

void MoveControl::stop() {
    mHasWanted = false;
}

void MoveControl::tick(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl) {
    mMoving = false;

    if (!mHasWanted)
        return;

    const Vector3f motion = mob.getMotion();
    const bool inWater = LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water;
    if (!jumpControl.isCoolingDown() && !mob.isOnGround() && !inWater && motion.y > 0.0f)
        return;

    const float speed = mob.getAttributes().get(MOVEMENT_ATTRIBUTE) * mSpeedModifier;
    if (motion.x * motion.x + motion.z * motion.z > speed * speed * SATURATED_SPEED_RATIO)
        return;

    const Vector3f position = mob.getPosition();
    const float relativeX = mWantedPosition.x - position.x;
    const float relativeZ = mWantedPosition.z - position.z;
    const float lengthSquared = relativeX * relativeX + relativeZ * relativeZ;
    if (lengthSquared < PRECISION) {
        stop();
        return;
    }

    const float length = std::sqrt(lengthSquared);
    const float scale = speed / length * SPEED_FACTOR;
    const float dx = relativeX * scale;
    const float dz = relativeZ * scale;

    if (mob.isOnGround() && !jumpControl.isCoolingDown())
        _tryJump(owner, mob, jumpControl, dx, dz);

    mob.setMotion(Vector3f(motion.x + dx, motion.y, motion.z + dz));
    mMoving = true;

    if (length < speed)
        stop();
}

void MoveControl::_tryJump(ServerNetworkHandler &owner, MobActor &mob, JumpControl &jumpControl, float dx,
                           float dz) const {
    const AxisAlignedBB movementBox = ActorPushSystem::boundingBoxOf(mob).offset(dx, 0.0f, dz);
    const std::vector<AxisAlignedBB> obstacles = owner.getLevelFor(mob).getCollisionBoxes(movementBox);

    float maxY = 0.0f;
    bool blocked = false;
    for (const AxisAlignedBB &obstacle: obstacles) {
        if (!movementBox.intersectsWith(obstacle))
            continue;

        blocked = true;
        if (obstacle.mMaxY > maxY)
            maxY = obstacle.mMaxY;
    }

    if (!blocked)
        return;

    const float height = maxY - mob.getPosition().y;
    if (height > MIN_JUMP_HEIGHT && height <= MAX_JUMP_HEIGHT)
        jumpControl.jump(height);
}

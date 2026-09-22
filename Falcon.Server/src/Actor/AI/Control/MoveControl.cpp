#include "Actor/AI/Control/MoveControl.h"

#include "Actor/AI/Control/JumpControl.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Block/BlockShape.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const float PRECISION = 1.0e-5f;
    const float SPEED_FACTOR = 0.33f;
    const float SATURATED_SPEED_RATIO = 0.4756f;
    const float MIN_JUMP_HEIGHT = 0.01f;
    const float MAX_JUMP_HEIGHT = 1.1f;
}

void MoveControl::setWantedPosition(const Vector3f &position, float speed) {
    mWantedPosition = position;
    mSpeed = speed;
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

    const float speed = mSpeed * mob.getMovementSpeedMultiplier();
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
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob).offset(dx, 0.0f, dz);
    Level &level = owner.getLevelFor(mob);

    const int32_t minX = (int32_t) std::floor(box.mMinX);
    const int32_t minY = std::max((int32_t) std::floor(box.mMinY) - 1, LevelChunk::MIN_Y);
    const int32_t minZ = (int32_t) std::floor(box.mMinZ);
    const int32_t maxX = (int32_t) std::floor(box.mMaxX);
    const int32_t maxY = std::min((int32_t) std::floor(box.mMaxY), LevelChunk::MAX_Y);
    const int32_t maxZ = (int32_t) std::floor(box.mMaxZ);

    float top = 0.0f;
    bool blocked = false;
    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t z = minZ; z <= maxZ; ++z) {
            for (int32_t y = minY; y <= maxY; ++y) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || !BlockShape::hasCollision(*state))
                    continue;

                const AxisAlignedBB shape = BlockShape::getShapeAt(*state, x, y, z);
                if (!shape.intersectsWith(box))
                    continue;

                blocked = true;
                if (shape.mMaxY > top)
                    top = shape.mMaxY;
            }
        }
    }

    if (!blocked)
        return;

    const float height = top - mob.getPosition().y;
    if (height > MIN_JUMP_HEIGHT && height <= MAX_JUMP_HEIGHT)
        jumpControl.jump(height);
}

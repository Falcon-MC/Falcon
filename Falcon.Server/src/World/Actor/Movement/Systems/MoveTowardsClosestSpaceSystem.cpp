#include "World/Actor/Movement/Systems/MoveTowardsClosestSpaceSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

using namespace MovementPhysics;

namespace {
constexpr float EPSILON = 1.1920929e-07f;
constexpr float MIN_LENGTH = 1.0e-4f;
constexpr float PUSH_SPEED = 0.1f;

float sign(float value) {
    return value > 0.0f ? 1.0f : value < 0.0f ? -1.0f : 0.0f;
}

bool overlapsAny(const std::vector<AxisAlignedBB> &boxes, float minX, float maxX, float minY, float maxY,
                 float minZ, float maxZ) {
    for (const AxisAlignedBB &box: boxes) {
        if (box.mMinX < maxX && minX < box.mMaxX && box.mMinY < maxY && minY < box.mMaxY && box.mMinZ < maxZ
            && minZ < box.mMaxZ)
            return true;
    }
    return false;
}

// the box grown by one block toward the push; blocked both ways: no push on that axis
float freeDirection(float push, float min, float max,
                    const std::function<bool(float, float)> &overlaps) {
    const float step = sign(push);
    if (!overlaps(step < 0.0f ? min + step : min, step > 0.0f ? max + step : max))
        return push;
    if (overlaps(step > 0.0f ? min - step : min, step < 0.0f ? max - step : max))
        return 0.0f;
    return -push;
}

void steer(float &velocity, float push) {
    const float limit = std::fabs(push);
    if (limit <= std::fabs(velocity))
        return;
    velocity = std::max(std::min(velocity + push, limit), -limit);
}
} // namespace

// MoveTowardsClosestSpaceSystemFromActor per actor (sub_1436273E0)
void MoveTowardsClosestSpaceSystem::tick(PlayerMovementContext &context) {
    const ActorFlags &flags = context.player.getFlags();
    if (!flags.get(ActorFlag::PushTowardsClosestSpace) || !flags.get(ActorFlag::HasCollision))
        return;

    auto &state = context.state;
    const AxisAlignedBB body = boxAt(state.start, context.pose.mHeight);
    const AxisAlignedBB area(body.mMinX - 1.0f, body.mMinY, body.mMinZ - 1.0f, body.mMaxX + 1.0f, body.mMaxY,
                             body.mMaxZ + 1.0f);
    const std::vector<AxisAlignedBB> boxes = context.level.getCollisionBoxes(area, &context.player);

    float centerX = 0.0f;
    float centerZ = 0.0f;
    size_t inside = 0;
    for (const AxisAlignedBB &box: boxes) {
        if (!(box.mMinX < body.mMaxX && body.mMinX < box.mMaxX && box.mMinY < body.mMaxY && body.mMinY < box.mMaxY
              && box.mMinZ < body.mMaxZ && body.mMinZ < box.mMaxZ))
            continue;
        centerX += box.mMinX + (box.mMaxX - box.mMinX) * 0.5f;
        centerZ += box.mMinZ + (box.mMaxZ - box.mMinZ) * 0.5f;
        ++inside;
    }
    if (inside == 0)
        return;

    centerX *= 1.0f / (float) inside;
    centerZ *= 1.0f / (float) inside;
    const float halfX = (body.mMaxX - body.mMinX) * 0.5f;
    const float halfZ = (body.mMaxZ - body.mMinZ) * 0.5f;
    float pushX = body.mMinX + halfX - centerX;
    float pushZ = body.mMinZ + halfZ - centerZ;
    // centered on the blocks: push diagonally
    if (std::fabs(pushX) < EPSILON && std::fabs(pushZ) < EPSILON) {
        pushX = 1.0f;
        pushZ = 1.0f;
    }

    // probes are a body-sized box on the blocks' center, not on the actor
    const float minX = std::min(centerX - halfX, centerX + halfX);
    const float maxX = std::max(centerX - halfX, centerX + halfX);
    const float minZ = std::min(centerZ - halfZ, centerZ + halfZ);
    const float maxZ = std::max(centerZ - halfZ, centerZ + halfZ);
    const float minY = std::min(body.mMinY, body.mMaxY);
    const float maxY = std::max(body.mMinY, body.mMaxY);

    pushX = freeDirection(pushX, minX, maxX, [&](float low, float high) {
        return overlapsAny(boxes, low, high, minY, maxY, minZ, maxZ);
    });
    pushZ = freeDirection(pushZ, minZ, maxZ, [&](float low, float high) {
        return overlapsAny(boxes, minX, maxX, minY, maxY, low, high);
    });

    if (std::fabs(pushX) < EPSILON && std::fabs(pushZ) < EPSILON)
        return;

    const float length = std::sqrt(pushX * pushX + pushZ * pushZ);
    if (length < MIN_LENGTH)
        return;
    steer(state.velocity.x, pushX / length * PUSH_SPEED);
    steer(state.velocity.z, pushZ / length * PUSH_SPEED);
}

#include "Actor/AI/Goal/MoveToWaterGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockData.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const BlockData *data = state == nullptr ? nullptr : BlockDataTable::find(state->mName.c_str());
        return data != nullptr && data->mSolid;
    }

    bool isOpen(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state != nullptr && !isSolid(level, x, y, z)
               && !LiquidBlocksFetch::at(level, Vector3f((float) x + 0.5f, (float) y, (float) z + 0.5f)).water;
    }
}

MoveToWaterGoal::MoveToWaterGoal(float speed, int32_t searchRange, int32_t searchHeight, float goalRadius,
                                 bool towardsWater)
        : mSpeed(speed), mSearchRange(searchRange), mSearchHeight(searchHeight), mGoalRadius(goalRadius),
          mTowardsWater(towardsWater) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveToWaterGoal::_isInWater(ServerNetworkHandler &owner, const MobActor &mob) const {
    return LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water;
}

bool MoveToWaterGoal::_findTarget(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &target) const {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    float nearest = -1.0f;
    for (int32_t x = originX - mSearchRange; x <= originX + mSearchRange; ++x) {
        for (int32_t y = originY - mSearchHeight; y <= originY + mSearchHeight; ++y) {
            for (int32_t z = originZ - mSearchRange; z <= originZ + mSearchRange; ++z) {
                const Vector3f candidate((float) x + 0.5f, (float) y, (float) z + 0.5f);
                const bool matches = mTowardsWater
                                     ? LiquidBlocksFetch::at(level, candidate).water
                                     : isSolid(level, x, y - 1, z) && isOpen(level, x, y, z) && isOpen(level, x, y + 1, z);
                if (!matches)
                    continue;

                const float dx = candidate.x - position.x;
                const float dy = candidate.y - position.y;
                const float dz = candidate.z - position.z;
                const float distance = dx * dx + dy * dy + dz * dz;
                if (nearest < 0.0f || distance < nearest) {
                    nearest = distance;
                    target = candidate;
                }
            }
        }
    }
    return nearest >= 0.0f;
}

bool MoveToWaterGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _isInWater(owner, mob) != mTowardsWater && _findTarget(owner, mob, mTarget);
}

bool MoveToWaterGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (_isInWater(owner, mob) == mTowardsWater || mob.getNavigation().isDone())
        return false;

    const Vector3f position = mob.getPosition();
    const float dx = mTarget.x - position.x;
    const float dy = mTarget.y - position.y;
    const float dz = mTarget.z - position.z;
    return dx * dx + dy * dy + dz * dz > mGoalRadius * mGoalRadius;
}

void MoveToWaterGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void MoveToWaterGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

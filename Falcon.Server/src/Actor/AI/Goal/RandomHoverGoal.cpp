#include "Actor/AI/Goal/RandomHoverGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockShape.h"
#include "Block/Blocks/LiquidView.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const int32_t TARGET_ATTEMPTS = 10;

    std::mt19937 &hoverRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomOffset(int32_t range) {
        return range <= 0 ? 0 : std::uniform_int_distribution<int32_t>(-range, range)(hoverRandom());
    }
}

RandomHoverGoal::RandomHoverGoal(const Settings &settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool RandomHoverGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mob.getNavigation().isDone())
        return false;

    if (mSettings.mInterval > 1
        && std::uniform_int_distribution<int32_t>(0, mSettings.mInterval - 1)(hoverRandom()) != 0)
        return false;

    return _pickTarget(owner.getLevelFor(mob), mob);
}

bool RandomHoverGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void RandomHoverGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSettings.mSpeed);
}

void RandomHoverGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

bool RandomHoverGoal::_pickTarget(Level &level, const MobActor &mob) {
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);
    const bool restricted = mSettings.mHomeRadius > 0.0f && mob.hasHome();

    for (int32_t attempt = 0; attempt < TARGET_ATTEMPTS; ++attempt) {
        const int32_t x = originX + randomOffset(mSettings.mHorizontalRange);
        const int32_t y = originY + randomOffset(mSettings.mVerticalRange) + mSettings.mVerticalOffset;
        const int32_t z = originZ + randomOffset(mSettings.mHorizontalRange);
        const Vector3f target((float) x + 0.5f, (float) y, (float) z + 0.5f);

        if (restricted) {
            const Vector3f &home = mob.getHomePosition();
            const float dx = target.x - home.x;
            const float dy = target.y - home.y;
            const float dz = target.z - home.z;
            if (dx * dx + dy * dy + dz * dz > mSettings.mHomeRadius * mSettings.mHomeRadius)
                continue;
        }

        if (!_hasHoverHeight(level, x, y, z))
            continue;

        mTarget = target;
        return true;
    }

    return false;
}

bool RandomHoverGoal::_hasHoverHeight(Level &level, int32_t x, int32_t y, int32_t z) const {
    const BlockState *target = level.peekBlockPtr(x, y, z);
    if (target == nullptr || BlockShape::hasCollision(*target) || LiquidView(*target).isLiquid())
        return false;

    if (mSettings.mMaxHoverHeight <= 0)
        return true;

    for (int32_t height = 1; height <= mSettings.mMaxHoverHeight; ++height) {
        const BlockState *below = level.peekBlockPtr(x, y - height, z);
        if (below == nullptr)
            return false;

        if (BlockShape::hasCollision(*below) || LiquidView(*below).isLiquid())
            return height >= mSettings.mMinHoverHeight;
    }

    return false;
}

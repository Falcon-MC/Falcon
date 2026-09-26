#include "Actor/AI/Goal/AvoidBlockGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Mob/MobActor.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

namespace {
    const float FLEE_DISTANCE = 8.0f;

    std::mt19937 &avoidRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

AvoidBlockGoal::AvoidBlockGoal(Settings settings) : mSettings(std::move(settings)) {
    mSettings.mTickInterval = std::max(1, mSettings.mTickInterval);
    mSettings.mMinSoundInterval = std::max(1, mSettings.mMinSoundInterval);
    mSettings.mMaxSoundInterval = std::max(mSettings.mMinSoundInterval, mSettings.mMaxSoundInterval);
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

int32_t AvoidBlockGoal::_nextSound() const {
    return std::uniform_int_distribution<int32_t>(mSettings.mMinSoundInterval,
                                                  mSettings.mMaxSoundInterval)(avoidRandom());
}

bool AvoidBlockGoal::_findBlock(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &block) const {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    std::vector<Vector3f> found;
    float nearestDistance = 0.0f;
    for (int32_t x = originX - mSettings.mSearchRange; x <= originX + mSettings.mSearchRange; ++x) {
        for (int32_t y = originY - mSettings.mSearchHeight; y <= originY + mSettings.mSearchHeight; ++y) {
            for (int32_t z = originZ - mSettings.mSearchRange; z <= originZ + mSettings.mSearchRange; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || mSettings.mBlocks.count(state->mName) == 0)
                    continue;

                const Vector3f candidate((float) x + 0.5f, (float) y, (float) z + 0.5f);
                if (mSettings.mRandomTarget) {
                    found.push_back(candidate);
                    continue;
                }

                const float dx = candidate.x - position.x;
                const float dy = candidate.y - position.y;
                const float dz = candidate.z - position.z;
                const float distance = dx * dx + dy * dy + dz * dz;
                if (found.empty() || distance < nearestDistance) {
                    found.assign(1, candidate);
                    nearestDistance = distance;
                }
            }
        }
    }

    if (found.empty())
        return false;

    block = found[std::uniform_int_distribution<size_t>(0, found.size() - 1)(avoidRandom())];
    return true;
}

bool AvoidBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (--mScanTicks > 0)
        return false;
    mScanTicks = mSettings.mTickInterval;

    Vector3f block;
    if (!_findBlock(owner, mob, block))
        return false;

    const Vector3f position = mob.getPosition();
    float awayX = position.x - block.x;
    float awayZ = position.z - block.z;
    const float length = std::sqrt(awayX * awayX + awayZ * awayZ);
    if (length < 0.001f) {
        awayX = 1.0f;
        awayZ = 0.0f;
    } else {
        awayX /= length;
        awayZ /= length;
    }

    mDestination = Vector3f(position.x + awayX * FLEE_DISTANCE, position.y, position.z + awayZ * FLEE_DISTANCE);
    return true;
}

bool AvoidBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void AvoidBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mSoundTicks = 0;
    mob.getNavigation().moveTo(mDestination, mSettings.mSprintSpeed);
}

void AvoidBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getNavigation().stop(mob);
    EntityEvents::fireTrigger(owner, mob, mSettings.mOnEscape.get());
}

void AvoidBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mSettings.mSound.empty() || --mSoundTicks > 0)
        return;

    mSoundTicks = _nextSound();
    mob.playDefinitionSound(owner, mSettings.mSound);
}

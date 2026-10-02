#include "Actor/AI/Goal/DoorInteractGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockState.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/Blocks/DoorOrientationBlock.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const float DOOR_REACH = 1.5f;
    const float SAMPLES_PER_BLOCK = 4.0f;
    const float BLOCK_CENTER = 0.5f;
    const float MIN_DIRECTION_SQUARED = 1.0e-4f;

    bool isDoor(const BlockState &state) {
        return VanillaBlocks::getAs<DoorOrientationBlock>(state.mName) != nullptr;
    }
}

bool DoorInteractGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _findDoor(owner.getLevelFor(mob), mob);
}

bool DoorInteractGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return !mPassed;
}

void DoorInteractGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    const Vector3f position = mob.getPosition();
    mPassed = false;
    mDirectionX = (float) mDoorPosition.x + BLOCK_CENTER - position.x;
    mDirectionZ = (float) mDoorPosition.z + BLOCK_CENTER - position.z;
}

void DoorInteractGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    const Vector3f position = mob.getPosition();
    const float dx = (float) mDoorPosition.x + BLOCK_CENTER - position.x;
    const float dz = (float) mDoorPosition.z + BLOCK_CENTER - position.z;
    if (mDirectionX * dx + mDirectionZ * dz < 0.0f)
        mPassed = true;
}

const BlockState *DoorInteractGoal::getDoorState(Level &level) const {
    const BlockState *state = level.peekBlockPtr(mDoorPosition.x, mDoorPosition.y, mDoorPosition.z);
    return state != nullptr && isDoor(*state) ? state : nullptr;
}

bool DoorInteractGoal::isDoorClosed(Level &level) const {
    const BlockState *state = getDoorState(level);
    return state != nullptr && !OpenableBlock::isOpen(*state);
}

bool DoorInteractGoal::_findDoor(Level &level, MobActor &mob) {
    if (mob.getNavigation().isDone() || !mob.getMoveControl().hasWanted())
        return false;

    const Vector3f position = mob.getPosition();
    const Vector3f &wanted = mob.getMoveControl().getWantedPosition();
    const float dx = wanted.x - position.x;
    const float dz = wanted.z - position.z;
    const float lengthSquared = dx * dx + dz * dz;
    if (lengthSquared < MIN_DIRECTION_SQUARED)
        return false;

    const float length = std::sqrt(lengthSquared);
    const float reach = std::min(length, DOOR_REACH);
    const int32_t samples = std::max(1, (int32_t) std::ceil(reach * SAMPLES_PER_BLOCK));
    const int32_t y = (int32_t) std::floor(position.y);

    for (int32_t i = 0; i <= samples; ++i) {
        const float fraction = reach * (float) i / (float) samples / length;
        const int32_t x = (int32_t) std::floor(position.x + dx * fraction);
        const int32_t z = (int32_t) std::floor(position.z + dz * fraction);
        const BlockState *state = level.peekBlockPtr(x, y, z);
        if (state == nullptr || !isDoor(*state) || !DoorBlock::isOpenableByHand(state->mName))
            continue;

        mDoorPosition = DoorBlock::lowerPosition(level, Vector3i(x, y, z), *state);
        return true;
    }
    return false;
}

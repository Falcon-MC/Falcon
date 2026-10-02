#include "Actor/AI/Goal/SilverfishGoals.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Blocks/InfestedBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const char *const GRIEFING_RULE = "mobgriefing";
    const int32_t MERGE_CHANCE = 10;
    const float MERGE_HEIGHT_OFFSET = 0.5f;
    const Vector3i MERGE_DIRECTIONS[] = {Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
                                         Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)};
    const int32_t WAKE_DELAY_TICKS = 20;
    const int32_t WAKE_RANGE_HORIZONTAL = 10;
    const int32_t WAKE_RANGE_VERTICAL = 5;

    std::mt19937 &silverfishRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t nextSearchOffset(int32_t offset) {
        return (offset <= 0 ? 1 : 0) - offset;
    }
}

SilverfishMergeWithStoneGoal::SilverfishMergeWithStoneGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool SilverfishMergeWithStoneGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getTarget(owner) != nullptr || !mob.getNavigation().isDone())
        return false;

    Level &level = owner.getLevelFor(mob);
    if (!level.getGameRules().getBool(GRIEFING_RULE)
        || std::uniform_int_distribution<int32_t>(0, MERGE_CHANCE - 1)(silverfishRandom()) != 0)
        return false;

    const size_t count = sizeof(MERGE_DIRECTIONS) / sizeof(MERGE_DIRECTIONS[0]);
    const Vector3i &direction = MERGE_DIRECTIONS[std::uniform_int_distribution<size_t>(0, count - 1)(silverfishRandom())];
    const Vector3f position = mob.getPosition();
    const Vector3i candidate((int32_t) std::floor(position.x) + direction.x,
                             (int32_t) std::floor(position.y + MERGE_HEIGHT_OFFSET) + direction.y,
                             (int32_t) std::floor(position.z) + direction.z);

    const BlockState *state = level.peekBlockPtr(candidate.x, candidate.y, candidate.z);
    if (state == nullptr || !InfestedBlock::infestedStateOf(*state, mInfestedState))
        return false;

    mPosition = candidate;
    return true;
}

bool SilverfishMergeWithStoneGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void SilverfishMergeWithStoneGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    owner.getLevelFor(mob).setBlock(mPosition, mInfestedState, true);
    mob.despawn();
}

bool SilverfishWakeUpFriendsGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mob.getHurtCount() == mHandledHurtCount)
        return false;

    mHandledHurtCount = mob.getHurtCount();
    return mob.getLastHurtBy() != 0;
}

bool SilverfishWakeUpFriendsGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mDelayTicks > 0;
}

void SilverfishWakeUpFriendsGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mDelayTicks = WAKE_DELAY_TICKS;
}

void SilverfishWakeUpFriendsGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (--mDelayTicks > 0)
        return;

    _wakeFriends(owner, mob);
}

void SilverfishWakeUpFriendsGoal::_wakeFriends(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const bool griefing = level.getGameRules().getBool(GRIEFING_RULE);
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    for (int32_t y = 0; y <= WAKE_RANGE_VERTICAL && y >= -WAKE_RANGE_VERTICAL; y = nextSearchOffset(y)) {
        for (int32_t x = 0; x <= WAKE_RANGE_HORIZONTAL && x >= -WAKE_RANGE_HORIZONTAL; x = nextSearchOffset(x)) {
            for (int32_t z = 0; z <= WAKE_RANGE_HORIZONTAL && z >= -WAKE_RANGE_HORIZONTAL; z = nextSearchOffset(z)) {
                const Vector3i blockPosition(originX + x, originY + y, originZ + z);
                const BlockState *state = level.peekBlockPtr(blockPosition.x, blockPosition.y, blockPosition.z);
                const InfestedBlock *infested = state == nullptr ? nullptr
                                                                 : VanillaBlocks::getAs<InfestedBlock>(state->mName);
                if (infested == nullptr)
                    continue;

                if (griefing)
                    infested->release(owner, level, blockPosition);
                else
                    level.setBlock(blockPosition, infested->hostStateOf(*state), true);

                if (std::uniform_int_distribution<int32_t>(0, 1)(silverfishRandom()) == 0)
                    return;
            }
        }
    }
}

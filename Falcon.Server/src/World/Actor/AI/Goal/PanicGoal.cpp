#include "World/Actor/AI/Goal/PanicGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    // a hurt panics for 40 ticks (sub_147C19E20); the path is checked every 8 (sub_147C19FC0)
    const int64_t PANIC_TICKS = 40;
    const int64_t REPATH_TICKS = 8;
    // RandomPos (sub_1439CCB90, sub_1439CE2D0): 5 out, 4 up or down, 10 tries
    const int32_t HORIZONTAL_RANGE = 5;
    const int32_t VERTICAL_RANGE = 4;
    const int32_t TRIES = 10;

    std::mt19937 &panicRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t nextInt(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(panicRandom());
    }
}

PanicGoal::PanicGoal(Settings settings) : mSettings(std::move(settings)) {
    // an empty list takes the game's default; "all" any cause (the list holding cause 35)
    if (mSettings.mDamageSources.empty())
        mSettings.mDamageSources = Settings().mDamageSources;
    mAnySource = std::find(mSettings.mDamageSources.begin(), mSettings.mDamageSources.end(), "all")
                 != mSettings.mDamageSources.end();
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool PanicGoal::_shouldPanic(ServerNetworkHandler &owner, MobActor &mob) const {
    const bool recent = owner.getCurrentTick() - mob.getLastHurtTick() <= PANIC_TICKS;
    Actor *attacker = mob.getLastHurtBy() == 0 ? nullptr : MobActor::findActor(owner, mob.getLastHurtBy());
    if (attacker != nullptr)
        return !mSettings.mIgnoreMobDamage && recent;
    if (!recent)
        return false;
    if (mAnySource)
        return true;
    for (const std::string &cause: mSettings.mDamageSources) {
        if (DamageCause::matches(cause, mob.getLastHurtSource()))
            return true;
    }
    return false;
}

std::optional<Vector3f> PanicGoal::_findTarget(ServerNetworkHandler &owner, MobActor &mob) const {
    // away from the attacker when there is one: an offset pointing toward it is mirrored (sub_1439CD170)
    Actor *attacker = mob.getLastHurtBy() == 0 ? nullptr : MobActor::findActor(owner, mob.getLastHurtBy());
    const Vector3f position = mob.getPosition();
    const bool away = attacker != nullptr;
    const float awayX = away ? position.x - attacker->getPosition().x : 0.0f;
    const float awayZ = away ? position.z - attacker->getPosition().z : 0.0f;

    Level &level = owner.getLevelFor(mob);
    std::optional<Vector3f> best;
    float bestValue = -99999.0f;
    for (int32_t attempt = 0; attempt < TRIES; attempt++) {
        int32_t dx = nextInt(2 * HORIZONTAL_RANGE + 1) - HORIZONTAL_RANGE;
        int32_t dz = nextInt(2 * HORIZONTAL_RANGE + 1) - HORIZONTAL_RANGE;
        const int32_t dy = nextInt(2 * VERTICAL_RANGE + 1) - VERTICAL_RANGE;
        if (away && (float) dz * awayZ + (float) dx * awayX < 0.0f) {
            dx = -dx;
            dz = -dz;
        }
        const int32_t x = (int32_t) std::floor(position.x) + dx;
        const int32_t z = (int32_t) std::floor(position.z) + dz;
        int32_t y = (int32_t) std::floor(position.y) + dy;
        const int32_t low = (int32_t) std::floor(position.y) - VERTICAL_RANGE;
        const int32_t high = (int32_t) std::floor(position.y) + VERTICAL_RANGE;
        // a walking mob's target stands on the ground: up out of solid blocks, down onto one
        while (y < high && BlockSupport::isSolid(level.getBlockState(x, y, z)))
            y++;
        while (y > low && !BlockSupport::isSolid(level.getBlockState(x, y - 1, z)))
            y--;
        if (BlockSupport::isSolid(level.getBlockState(x, y, z)))
            continue;

        // Animal's walk target value: grass beneath is worth 10
        const float value = level.getBlockState(x, y - 1, z).mName == BlockNames::GRASS_BLOCK ? 10.0f : 0.0f;
        if (value > bestValue) {
            bestValue = value;
            best = Vector3f((float) x, (float) y, (float) z);
        }
    }
    return best;
}

bool PanicGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mSettings.mForce && !_shouldPanic(owner, mob))
        return false;
    const std::optional<Vector3f> target = _findTarget(owner, mob);
    if (!target)
        return false;
    mTarget = *target;
    return true;
}

bool PanicGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _shouldPanic(owner, mob) || !mob.getNavigation().isDone();
}

void PanicGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getNavigation().moveTo(mTarget, mSettings.mSpeed);
    mNextRepathTick = owner.getCurrentTick() + REPATH_TICKS;
    mob.setPanicking(true);
}

void PanicGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.setPanicking(false);
}

void PanicGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mNextRepathTick)
        return;
    mNextRepathTick = owner.getCurrentTick() + REPATH_TICKS;
    if (!mob.getNavigation().isDone())
        return;
    const std::optional<Vector3f> target = _findTarget(owner, mob);
    if (!target)
        return;
    mTarget = *target;
    mob.getNavigation().moveTo(mTarget, mSettings.mSpeed);
}

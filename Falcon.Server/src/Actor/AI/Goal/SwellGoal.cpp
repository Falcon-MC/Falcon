#include "Actor/AI/Goal/SwellGoal.h"

#include "Actor/Mob/Hostile/CreeperActor.h"
#include "Actor/ServerPlayer.h"
#include "Level/Explosion.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const float START_RANGE_SQUARED = 9.0f;
    const float CANCEL_RANGE_SQUARED = 49.0f;
    const double SIGHT_STEP = 0.25;
    const float EYE_RATIO = 0.85f;
    const float PLAYER_EYE_HEIGHT = 1.62f;

    bool canSee(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
        const Vector3f from = mob.getPosition();
        const Vector3f to = target.getPosition();
        return !Explosion::isRayCollidingWithBlocks(owner.getLevelFor(mob), from.x,
                                                    from.y + mob.getSize().mHeight * EYE_RATIO, from.z, to.x,
                                                    to.y + PLAYER_EYE_HEIGHT, to.z, SIGHT_STEP);
    }
}

SwellGoal::SwellGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool SwellGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    CreeperActor *creeper = dynamic_cast<CreeperActor *>(&mob);
    if (creeper == nullptr)
        return false;

    const Actor *target = mob.getTarget(owner);
    return creeper->getSwell() > 0 || (target != nullptr && mob.distanceSquaredTo(*target) < START_RANGE_SQUARED);
}

void SwellGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;

    mob.getNavigation().stop(mob);
}

void SwellGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;

    CreeperActor *creeper = dynamic_cast<CreeperActor *>(&mob);
    if (creeper != nullptr)
        creeper->setSwellDirection(-1);
}

void SwellGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    CreeperActor *creeper = dynamic_cast<CreeperActor *>(&mob);
    if (creeper == nullptr)
        return;

    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || mob.distanceSquaredTo(*target) > CANCEL_RANGE_SQUARED || !canSee(owner, mob, *target)) {
        creeper->setSwellDirection(-1);
        return;
    }

    mob.getLookControl().setLookAt(target->getPosition());
    creeper->setSwellDirection(1);
}

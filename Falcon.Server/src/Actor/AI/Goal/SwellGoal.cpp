#include "Actor/AI/Goal/SwellGoal.h"

#include "Actor/Mob/Hostile/CreeperActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const float START_RANGE_SQUARED = 9.0f;
    const float CANCEL_RANGE_SQUARED = 49.0f;
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
    if (target == nullptr || mob.distanceSquaredTo(*target) > CANCEL_RANGE_SQUARED || !mob.canSee(owner, *target)) {
        creeper->setSwellDirection(-1);
        return;
    }

    mob.getLookControl().setLookAt(target->getPosition());
    creeper->setSwellDirection(1);
}

#include "Actor/AI/Goal/OwnerTargetGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

OwnerTargetGoal::OwnerTargetGoal(Mode mode) : mMode(mode) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

ServerPlayer *OwnerTargetGoal::_findOwner(ServerNetworkHandler &owner, const MobActor &mob) const {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.getName() == mob.getTamedBy() && player.isSpawned() && player.getDimension() == mob.getDimension())
            return &player;
    }
    return nullptr;
}

bool OwnerTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mob.isTamed() || mob.isSitting())
        return false;

    const ServerPlayer *player = _findOwner(owner, mob);
    if (player == nullptr)
        return false;

    const bool hurtBy = mMode == Mode::OwnerHurtBy;
    const uint64_t runtimeId = hurtBy ? player->getLastHurtByRuntimeId() : player->getLastAttackedRuntimeId();
    const int64_t tick = hurtBy ? player->getLastHurtByTick() : player->getLastAttackedTick();
    if (runtimeId == 0 || tick == mHandledTick)
        return false;

    const Actor *target = MobActor::findActor(owner, runtimeId);
    const MobActor *pet = dynamic_cast<const MobActor *>(target);
    if (target == nullptr || !mob.canTarget(*target) || (pet != nullptr && pet->getTamedBy() == mob.getTamedBy())) {
        mHandledTick = tick;
        return false;
    }

    mPendingTarget = runtimeId;
    mPendingTick = tick;
    return true;
}

bool OwnerTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mob.isSitting() && mob.getTarget(owner) != nullptr;
}

void OwnerTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mHandledTick = mPendingTick;
    mob.setTarget(owner, mPendingTarget);
}

void OwnerTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}

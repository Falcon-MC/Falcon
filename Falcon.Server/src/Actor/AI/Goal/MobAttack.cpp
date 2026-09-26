#include "Actor/AI/Goal/MobAttack.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *const DEATH_MESSAGE = "death.attack.mob";
}

bool MobAttack::hit(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float damage) {
    if (damage <= 0.0f)
        return false;

    const float healthBefore = target.getHealth();
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target)) {
        owner.hurt(*player, damage, ActorDamageSource::attack(DEATH_MESSAGE, player->getName(), mob, mob.getName(),
                                                              mob.getPosition()));
    } else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target)) {
        owner.damageActor(*actor, damage, &mob);
    }

    if (target.getHealth() >= healthBefore)
        return false;

    mob.getAnger().onAttack(owner, mob, target);
    return true;
}

bool MobAttack::isTouching(const MobActor &mob, const Actor &target, float reach) {
    const AxisAlignedBB reachBox = ActorPushSystem::boundingBoxOf(mob).expand(reach, reach, reach);
    return reachBox.intersectsWith(ActorPushSystem::boundingBoxOf(target));
}

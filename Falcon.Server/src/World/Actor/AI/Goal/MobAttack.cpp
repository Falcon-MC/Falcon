#include "World/Actor/AI/Goal/MobAttack.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/MobEffect.h"

#include <algorithm>

namespace {
    const char *const DEATH_MESSAGE = "death.attack.mob";
    const char *const ATTACK_COMPONENT = "minecraft:attack";
    const float TICKS_PER_SECOND = 20.0f;
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
    applyAttackEffect(owner, mob, target);
    return true;
}

bool MobAttack::isTouching(const MobActor &mob, const Actor &target, float reach) {
    const AxisAlignedBB reachBox = ActorPushSystem::boundingBoxOf(mob).expand(reach, reach, reach);
    return reachBox.intersectsWith(ActorPushSystem::boundingBoxOf(target));
}

bool MobAttack::attackEffect(const MobActor &mob, MobEffectInstance &effect) {
    // effect_name, effect_duration in seconds and effect_amplifier defaulting to 0; sub_147C33930
    const json::Value *attack = mob.getComponent(ATTACK_COMPONENT);
    const json::Value *name = attack == nullptr ? nullptr : attack->get("effect_name");
    if (name == nullptr || !parseDefinitionMobEffect(name->string(), effect.mId))
        return false;

    const json::Value *duration = attack->get("effect_duration");
    const json::Value *amplifier = attack->get("effect_amplifier");
    // truncated to ticks, never below 0; sub_1437DA310
    effect.mDuration = duration == nullptr ? 0
                                           : std::max(0, (int32_t) ((float) duration->number(0.0) * TICKS_PER_SECOND));
    effect.mAmplifier = amplifier == nullptr ? 0 : amplifier->integer(0);
    // the easy cave spider poison group sets a zero duration: no ailment at all
    return effect.mDuration > 0;
}

void MobAttack::applyAttackEffect(ServerNetworkHandler &owner, const MobActor &mob, Actor &target) {
    // a fatal hit already cleared the victim's effects: one added now would survive the respawn
    if (!target.isAlive() || target.isDead())
        return;

    MobEffectInstance effect;
    if (attackEffect(mob, effect))
        owner.addEffect(target, effect.mId, effect.mAmplifier, effect.mDuration);
}

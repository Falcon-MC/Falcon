#pragma once

class Actor;
class MobActor;
class ServerNetworkHandler;
struct MobEffectInstance;

class MobAttack {
public:
    static bool hit(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float damage);

    static bool isTouching(const MobActor &mob, const Actor &target, float reach);

    // the status ailment of the mob's minecraft:attack, false when it has none to give
    static bool attackEffect(const MobActor &mob, MobEffectInstance &effect);

    static void applyAttackEffect(ServerNetworkHandler &owner, const MobActor &mob, Actor &target);
};

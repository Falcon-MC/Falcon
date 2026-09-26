#pragma once

class Actor;
class MobActor;
class ServerNetworkHandler;

class MobAttack {
public:
    static bool hit(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float damage);

    static bool isTouching(const MobActor &mob, const Actor &target, float reach);
};

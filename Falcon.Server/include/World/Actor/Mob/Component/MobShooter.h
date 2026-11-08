#pragma once

#include <cstdint>

class Actor;
class MobActor;
class ServerNetworkHandler;

// minecraft:shooter: spawns the projectile entity the mob's definition names and aims it at the target
class MobShooter {
public:
    static bool canShoot(const MobActor &mob);

    static bool shootAt(ServerNetworkHandler &owner, MobActor &mob, const Actor &target);

    // degrees, negative up: lowest pitch whose arc reaches the target, drag ignored as the game does
    static float aimPitch(float horizontal, float height, float power, float gravity);

    static float uncertainty(float base, float multiplier, int32_t difficulty);
};

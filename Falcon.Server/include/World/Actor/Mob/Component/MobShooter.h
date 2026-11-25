#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <random>
#include <string>

class Actor;
class MobActor;
class ServerActor;
class ServerNetworkHandler;

// minecraft:shooter: spawns the projectile entity the mob's definition names and aims it at the target
class MobShooter {
public:
    static bool shootAt(ServerNetworkHandler &owner, MobActor &mob, const Actor &target);

    // the definition's projectile from a point, straight along a direction, with its power and uncertainty: the
    // way behavior.fire_at_target shoots
    static ServerActor *shootFrom(ServerNetworkHandler &owner, MobActor &mob, const std::string &identifier,
                                  const Vector3f &origin, const Vector3f &direction);

    // degrees, negative up: lowest pitch whose arc reaches the target, drag ignored as the game does
    static float aimPitch(float horizontal, float height, float power, float gravity);

    static float uncertainty(float base, float multiplier, int32_t difficulty);

    // impact_damage.damage: a number or a min/max range; -1 when it gives no fixed damage
    static int32_t impactDamage(const json::Value &impact, std::mt19937 &random);
};

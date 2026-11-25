#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class MobActor;
class ServerActor;
class ServerNetworkHandler;

// minecraft:teleport: random teleports, by light, toward a far target, and away from a projectile about to hit
class MobTeleport {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    // true when the mob teleported out of the projectile's way: the projectile keeps flying
    bool dodge(ServerNetworkHandler &owner, MobActor &mob, const ServerActor &projectile);

    // one try at a random spot of random_teleport_cube
    bool randomTeleport(ServerNetworkHandler &owner, MobActor &mob);

private:
    bool _teleportTowards(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &target);

    // ticks to the next random teleport; 0 at spawn, so the first one comes at once
    int32_t mCountdown = 0;
};

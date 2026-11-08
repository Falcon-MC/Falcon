#pragma once

#include "Core/Json/Json.h"

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

// minecraft:explode: a lit fuse counts down, then the actor explodes and is removed
class MobExplode {
public:
    // false once the actor exploded
    bool tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    void _setIgnited(ServerNetworkHandler &owner, MobActor &mob, bool ignited);

    void _explode(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component);

    const json::Value *mComponent = nullptr;
    int32_t mFuseTicks = -1;
    bool mExploded = false;
};

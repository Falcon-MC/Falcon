#pragma once

#include "Core/Json/Json.h"

#include <string>
#include <vector>

class MobActor;
class ServerNetworkHandler;

// minecraft:addrider: riders spawn when the definition is added, as the component's add hook does;
// groups restored from a save aren't added again: no riders
class MobAddRider {
public:
    void queue(const json::Value *component);

    // a rider unable to ride the mob is removed
    void spawnPending(ServerNetworkHandler &owner, MobActor &vehicle);

private:
    static void _spawnRider(ServerNetworkHandler &owner, MobActor &vehicle, const std::string &identifier,
                            const std::string &event);

    std::vector<const json::Value *> mPending;
};

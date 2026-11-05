#pragma once

#include "Core/Json/Json.h"

#include <string>
#include <vector>

class MobActor;
class ServerNetworkHandler;

/**
 * `minecraft:addrider`: the riders listed by a definition or component group are spawned on the mob and seated when
 * that definition is added, as the component's add hook does in the game. Groups restored from a save are not added
 * again, so they spawn no riders.
 */
class MobAddRider {
public:
    /** Queues the riders of an `minecraft:addrider` component that was just added. */
    void queue(const json::Value *component);

    /** Spawns the queued riders, each mounted on the mob or removed when it cannot ride it. */
    void spawnPending(ServerNetworkHandler &owner, MobActor &vehicle);

private:
    static void _spawnRider(ServerNetworkHandler &owner, MobActor &vehicle, const std::string &identifier,
                            const std::string &event);

    std::vector<const json::Value *> mPending;
};

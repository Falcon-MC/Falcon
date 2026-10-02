#pragma once

#include "Actor/ActorSize.h"

class Actor;
class ServerActor;
class ServerNetworkHandler;
class ServerPlayer;

class BlockContactSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);

    static void tick(ServerNetworkHandler &owner, ServerActor &actor);

    static void land(ServerNetworkHandler &owner, Actor &actor, float fallDistance);

private:
    static bool touchBlocks(ServerNetworkHandler &owner, Actor &actor, const ActorSize &size);

    static bool touchInsideBlocks(ServerNetworkHandler &owner, Actor &actor, const ActorSize &size);
};

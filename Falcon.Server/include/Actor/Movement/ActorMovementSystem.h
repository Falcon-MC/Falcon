#pragma once

class ServerActor;
class ServerNetworkHandler;

class ActorMovementSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);

private:
    static void _syncMovement(ServerNetworkHandler &owner, ServerActor &actor);
};

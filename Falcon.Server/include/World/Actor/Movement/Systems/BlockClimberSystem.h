#pragma once

class ServerActor;
class ServerNetworkHandler;

// minecraft:block_climber: the scaffolding and ascendable block flags viewers move the actor with
class BlockClimberSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);
};

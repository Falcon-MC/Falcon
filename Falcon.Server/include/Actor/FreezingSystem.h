#pragma once

class Actor;
class Level;
class ServerNetworkHandler;

class FreezingSystem {
public:
    static bool isSuppressed(Level &level, const Actor &actor);

    static void tick(ServerNetworkHandler &owner, Actor &actor);
};

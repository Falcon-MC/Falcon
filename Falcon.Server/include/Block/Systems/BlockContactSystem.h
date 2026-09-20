#pragma once

class ServerNetworkHandler;
class ServerPlayer;

class BlockContactSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);
};

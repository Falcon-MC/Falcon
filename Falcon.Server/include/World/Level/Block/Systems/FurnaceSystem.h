#pragma once

class ServerNetworkHandler;
class ServerPlayer;

class FurnaceSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);
};

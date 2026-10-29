#pragma once

#include <string>

class ServerNetworkHandler;
class ServerPlayer;

class ElytraItem {
public:
    static constexpr const char *IDENTIFIER = "minecraft:elytra";

    static bool isWearingElytra(const ServerPlayer &player);

    static void tickGliding(ServerNetworkHandler &owner, ServerPlayer &player);
};

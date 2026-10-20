#pragma once

#include <string>

class ServerNetworkHandler;
class ServerPlayer;
class ItemStack;

class TotemItem {
public:
    static constexpr const char *IDENTIFIER = "minecraft:totem_of_undying";

    static bool matches(const ItemStack &item);

    static bool consume(ServerNetworkHandler &owner, ServerPlayer &player);
};

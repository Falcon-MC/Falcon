#pragma once

#include "Core/Math/Vector3f.h"

class ItemStack;
class Level;
class ServerNetworkHandler;
class ServerPlayer;

class ChorusFruitItem {
public:
    static bool isChorusFruit(const ItemStack &item);

    static bool canConsume(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool onEaten(ServerNetworkHandler &owner, ServerPlayer &player);

private:
    static bool isSolid(Level &level, int x, int y, int z);

    static bool isLiquid(Level &level, int x, int y, int z);

    static bool findTeleportPosition(ServerNetworkHandler &owner, ServerPlayer &player, Vector3f &destination);

};

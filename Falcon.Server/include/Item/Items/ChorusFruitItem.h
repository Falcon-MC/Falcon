#pragma once

#include "Core/Math/Vector3f.h"
#include "Item/Item.h"

class ItemStack;
class Level;
class ServerNetworkHandler;
class ServerPlayer;

class ChorusFruitItem : public Item {
public:
    explicit ChorusFruitItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool canAlwaysEat() const override {
        return true;
    }

    static bool isChorusFruit(const ItemStack &item);

    static bool canConsume(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool onEaten(ServerNetworkHandler &owner, ServerPlayer &player);

private:
    static bool isSolid(Level &level, int x, int y, int z);

    static bool isLiquid(Level &level, int x, int y, int z);

    static bool findTeleportPosition(ServerNetworkHandler &owner, ServerPlayer &player, Vector3f &destination);

};

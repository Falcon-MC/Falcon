#pragma once

#include "Core/Math/Vector3f.h"
#include "Item/Item.h"

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

    bool canConsume(ServerNetworkHandler &owner, ServerPlayer &player) const override;

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

private:
    static bool isSolid(Level &level, int x, int y, int z);

    static bool isLiquid(Level &level, int x, int y, int z);

    static bool findTeleportPosition(ServerNetworkHandler &owner, ServerPlayer &player, Vector3f &destination);

};

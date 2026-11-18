#pragma once

#include "World/Item/Item.h"

#include <string>

class ServerActor;

class FireworkRocketItem : public Item {
public:
    explicit FireworkRocketItem(const Item &base);

    // a rocket from the item's Fireworks data, the default star without it, rising from position
    static ServerActor *launch(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                               const ItemStack &item);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

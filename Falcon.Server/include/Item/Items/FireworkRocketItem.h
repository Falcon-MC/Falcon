#pragma once

#include "Item/Item.h"

#include <string>

class FireworkRocketItem : public Item {
public:
    explicit FireworkRocketItem(const Item &base);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

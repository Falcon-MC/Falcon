#pragma once

#include "World/Item/Item.h"

// every minecart item: it puts its minecart on the clicked rail
class MinecartItem : public Item {
public:
    explicit MinecartItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;

    bool dispense(const DispenseSource &source) const override;
};

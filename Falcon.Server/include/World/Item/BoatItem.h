#pragma once

#include "World/Item/Item.h"

// every boat and chest boat item, bamboo rafts included: the item puts its boat where the player clicks
class BoatItem : public Item {
public:
    explicit BoatItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

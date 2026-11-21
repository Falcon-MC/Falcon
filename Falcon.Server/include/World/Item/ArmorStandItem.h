#pragma once

#include "World/Item/Item.h"

class ArmorStandItem : public Item {
public:
    explicit ArmorStandItem(const Item &base);

    static bool matches(const std::string &identifier);

    // nearest eighth of a turn, facing the player
    static float placedYaw(float playerYaw);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

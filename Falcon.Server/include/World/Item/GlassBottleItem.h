#pragma once

#include "World/Item/Item.h"

#include <string>

class GlassBottleItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:glass_bottle";

    explicit GlassBottleItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

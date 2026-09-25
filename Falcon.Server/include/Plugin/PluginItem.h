#pragma once

#include "Item/Item.h"

#include <string>

class PluginItem : public Item {
public:
    explicit PluginItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

#pragma once

#include "World/Item/Item.h"

#include <string>

class BundleItem : public Item {
public:
    explicit BundleItem(const Item &base);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    static bool matches(const std::string &identifier);
};

#pragma once

#include "World/Item/Item.h"

#include <string>

class GoatHornItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:goat_horn";

    explicit GoatHornItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

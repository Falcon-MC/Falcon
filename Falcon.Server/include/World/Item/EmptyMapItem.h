#pragma once

#include "World/Item/Item.h"

#include <string>

// minecraft:empty_map; aux 2 is the empty locator map
class EmptyMapItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:empty_map";

    explicit EmptyMapItem(const Item &base);

    static bool matches(const std::string &identifier);

    // EmptyMapItem::use (sub_147B9F900)
    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

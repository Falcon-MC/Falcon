#pragma once

#include "Item/Item.h"

class BowItem : public Item {
public:
    explicit BowItem(const Item &base);

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;
};

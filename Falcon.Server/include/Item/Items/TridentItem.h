#pragma once

#include "Item/Item.h"

class TridentItem : public Item {
public:
    explicit TridentItem(const Item &base);

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

private:
    bool applyRiptide(ServerNetworkHandler &owner, ServerPlayer &player, int32_t level) const;
};

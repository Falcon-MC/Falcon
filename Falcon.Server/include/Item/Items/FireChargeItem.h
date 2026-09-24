#pragma once

#include "Item/Item.h"

class FireChargeItem : public Item {
public:
    explicit FireChargeItem(const Item &base);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

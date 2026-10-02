#pragma once

#include "Item/Item.h"

class FlintAndSteelItem : public Item {
public:
    explicit FlintAndSteelItem(const Item &base);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

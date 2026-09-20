#pragma once

#include "Item/Item.h"

class EndCrystalItem : public Item {
public:
    explicit EndCrystalItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

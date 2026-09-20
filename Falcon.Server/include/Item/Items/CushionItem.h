#pragma once

#include "Item/Item.h"

class CushionItem final : public Item {
public:
    static constexpr const char *SUFFIX = "_cushion";

    explicit CushionItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face,
                      const Vector3f &clickPosition) const override;
};

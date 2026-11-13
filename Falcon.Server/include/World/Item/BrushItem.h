#pragma once

#include "World/Item/Item.h"

class BrushItem final : public Item {
public:
    explicit BrushItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;

    // BrushItem ctor (sub_143F9ACC0, 26.60)
    int32_t getUseDuration() const override {
        return 72000;
    }
};

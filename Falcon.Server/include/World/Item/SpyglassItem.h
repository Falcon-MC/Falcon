#pragma once

#include "World/Item/Item.h"

#include <string>

class SpyglassItem final : public Item {
public:
    explicit SpyglassItem(const Item &base);

    static bool matches(const std::string &identifier);

    int32_t getUseDuration(const ItemStack &item) const override {
        (void) item;
        return 72000;
    }

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;
};

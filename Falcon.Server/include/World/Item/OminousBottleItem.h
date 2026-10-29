#pragma once

#include "World/Item/Item.h"

#include <string>

class OminousBottleItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ominous_bottle";

    static constexpr int32_t BAD_OMEN_DURATION_TICKS = 120000;
    static constexpr int32_t MAX_AMPLIFIER = 4;

    explicit OminousBottleItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool isConsumable() const override {
        return true;
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

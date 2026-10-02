#pragma once

#include "Item/Item.h"

class GoldenAppleItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:golden_apple";

    static constexpr int32_t ABSORPTION_DURATION_TICKS = 2400;
    static constexpr int32_t REGENERATION_DURATION_TICKS = 100;
    static constexpr int32_t REGENERATION_AMPLIFIER = 1;

    explicit GoldenAppleItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool canAlwaysEat() const override {
        return true;
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

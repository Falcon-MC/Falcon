#pragma once

#include "Item/Item.h"

class EnchantedGoldenAppleItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:enchanted_golden_apple";

    static constexpr int32_t REGENERATION_DURATION_TICKS = 600;
    static constexpr int32_t REGENERATION_AMPLIFIER = 1;
    static constexpr int32_t ABSORPTION_DURATION_TICKS = 2400;
    static constexpr int32_t ABSORPTION_AMPLIFIER = 3;
    static constexpr int32_t RESISTANCE_DURATION_TICKS = 6000;
    static constexpr int32_t FIRE_RESISTANCE_DURATION_TICKS = 6000;

    explicit EnchantedGoldenAppleItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool canAlwaysEat() const override {
        return true;
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

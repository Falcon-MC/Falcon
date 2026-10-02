#pragma once

#include "Item/Item.h"

class PotionItem : public Item {
public:
    explicit PotionItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool isConsumable() const override {
        return true;
    }

    std::string getUsingConvertsTo() const override {
        return "minecraft:glass_bottle";
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

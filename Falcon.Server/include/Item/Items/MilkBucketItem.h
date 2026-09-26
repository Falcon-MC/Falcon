#pragma once

#include "Item/Item.h"

class MilkBucketItem : public Item {
public:
    explicit MilkBucketItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool isConsumable() const override {
        return true;
    }

    std::string getUsingConvertsTo() const override {
        return "minecraft:bucket";
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

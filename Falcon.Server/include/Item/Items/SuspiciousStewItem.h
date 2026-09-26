#pragma once

#include "Item/Item.h"

class SuspiciousStewItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:suspicious_stew";

    explicit SuspiciousStewItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool canAlwaysEat() const override {
        return true;
    }

    void onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

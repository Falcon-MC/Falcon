#pragma once

#include "World/Item/Item.h"

#include <string>

class FishingHookActor;

class FishingRodItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:fishing_rod";

    explicit FishingRodItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

private:
    static FishingHookActor *_findHook(ServerNetworkHandler &owner, const ServerPlayer &player);
};

#pragma once

#include "World/Item/ThrowableItem.h"

#include <string>

class ThrownPotionItem : public ThrowableItem {
public:
    ThrownPotionItem(const Item &base, std::string entityIdentifier);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

#pragma once

#include "World/Item/Item.h"

#include <string>

class SpawnEggItem : public Item {
public:
    explicit SpawnEggItem(const Item &base);

    // the actor an egg item spawns; empty for an item that is no egg
    static std::string spawnedActorOf(const std::string &itemIdentifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};

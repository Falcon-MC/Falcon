#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class EventBus;
class ServerPlayer;

class ItemDurability {
public:
    // bus: the events go there when given, else to the player's bus
    static bool apply(ServerPlayer *player, ItemStack &item, int32_t amount, bool rollUnbreaking = true,
                      EventBus *bus = nullptr);

private:
    static int32_t _rollUnbreaking(const ItemStack &item, int32_t amount);
};

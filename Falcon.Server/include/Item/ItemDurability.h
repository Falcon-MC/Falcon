#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class ServerPlayer;

class ItemDurability {
public:
    static bool apply(ServerPlayer *player, ItemStack &item, int32_t amount, bool rollUnbreaking = true);

private:
    static int32_t _rollUnbreaking(const ItemStack &item, int32_t amount);
};

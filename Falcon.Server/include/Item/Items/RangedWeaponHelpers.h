#pragma once

#include "Inventory/PlayerInventory.h"

#include <cstdint>

class ServerNetworkHandler;
class ServerPlayer;

namespace RangedWeaponHelpers {
    extern const char *ARROW_IDENTIFIER;
    extern const char *ARROW_ACTOR;

    const float ARROW_BASE_DAMAGE = 2.0f;
    const int OFFHAND_SLOT = PlayerInventory::CONTAINER_SIZE;

    bool hasFiniteResources(const ServerPlayer &player);

    int findArrowSlot(const ServerPlayer &player);

    const ItemStack &arrowAt(const ServerPlayer &player, int slot);

    void consumeArrow(ServerNetworkHandler &owner, ServerPlayer &player, int slot);

    float chargeForce(int32_t elapsedTicks, float maxForce);
}

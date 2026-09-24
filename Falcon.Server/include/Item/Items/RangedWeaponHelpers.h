#pragma once

#include <cstdint>

class ServerPlayer;

namespace RangedWeaponHelpers {
    extern const char *ARROW_IDENTIFIER;
    extern const char *ARROW_ACTOR;

    const float ARROW_BASE_DAMAGE = 2.0f;

    bool hasFiniteResources(const ServerPlayer &player);

    int findArrowSlot(const ServerPlayer &player);

    void consumeArrow(ServerPlayer &player, int slot);

    float chargeForce(int32_t elapsedTicks, float maxForce);
}

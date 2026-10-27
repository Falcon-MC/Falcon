#pragma once

#include "Core/Math/Vector3i.h"

class Level;
class ServerNetworkHandler;

class PrecipitationSystem {
public:
    static void tick(ServerNetworkHandler &owner, Level &level);

    static bool isCold(Level &level, const Vector3i &position);

private:
    static void tickColumn(Level &level, int32_t x, int32_t z);

    static bool shouldFreeze(Level &level, const Vector3i &position);

    static bool shouldSnow(Level &level, const Vector3i &position);
};

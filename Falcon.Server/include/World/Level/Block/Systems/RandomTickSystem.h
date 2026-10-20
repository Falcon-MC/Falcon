#pragma once

#include "Core/Math/Vector3i.h"

class Level;
class ServerNetworkHandler;

class RandomTickSystem {
public:
    static void tick(ServerNetworkHandler &owner, Level &level);

    static int getFullLight(Level &level, const Vector3i &position);

    static int getBlockLight(Level &level, const Vector3i &position);

    static int nextInt(int bound);
};

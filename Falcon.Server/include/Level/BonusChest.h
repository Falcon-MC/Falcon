#pragma once

#include "Core/Math/Vector3i.h"

class Level;
class ServerNetworkHandler;

class BonusChest {
public:
    static bool placeIfPending(ServerNetworkHandler &owner, Level &level);

private:
    static bool _findPosition(Level &level, const Vector3i &around, Vector3i &out);

    static void _fill(ServerNetworkHandler &owner, Level &level, const Vector3i &position);
};

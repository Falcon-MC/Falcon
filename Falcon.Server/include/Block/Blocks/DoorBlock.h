#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <string>

class Level;
class ServerNetworkHandler;

class DoorBlock {
public:
    static bool matches(const std::string &identifier);

    static bool isRightHinged(Level *level, const std::string &identifier, const Vector3i &position,
                              int playerFacing);

    static Vector3i lowerPosition(Level &level, const Vector3i &position, const BlockState &state);

    static bool isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

    static bool toggle(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                       const BlockState &state);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);
};

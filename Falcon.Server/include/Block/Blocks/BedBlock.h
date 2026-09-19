#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <string>

class Level;
class ServerNetworkHandler;

class BedBlock {
public:
    static bool matches(const std::string &identifier);

    static void placeHeadPiece(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state,
                               int playerFacing);
};

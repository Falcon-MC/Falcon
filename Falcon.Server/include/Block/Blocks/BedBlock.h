#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <string>

class Level;
class ServerNetworkHandler;
class ServerPlayer;

class BedBlock {
public:
    static bool matches(const std::string &identifier);

    static void placeHeadPiece(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state, int playerFacing);

    static bool findHead(Level &level, const Vector3i &position, const BlockState &state, Vector3i &head);

    static bool isValidAt(Level &level, const Vector3i &head);

    static void setOccupied(ServerNetworkHandler &owner, Level &level, const Vector3i &head, bool occupied);

    static bool use(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state);

    static void breakOtherHalf(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

private:
    static int headFace(const BlockState &state);
};

#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <string>
#include <vector>

class Level;
class ServerNetworkHandler;
class ServerPlayer;

class BedBlock {
public:
    static bool matches(const std::string &identifier);

    static bool findHead(Level &level, const Vector3i &position, const BlockState &state, Vector3i &head);

    static bool isValidAt(Level &level, const Vector3i &head);

    static void setOccupied(ServerNetworkHandler &owner, Level &level, const Vector3i &head, bool occupied);

    static bool use(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state);

    // where a sleeper stands up; previous is where they stood when lying down
    static bool findStandUpPosition(Level &level, const Vector3i &bed, const Vector3f &previous, Vector3i &standUp);

    static std::vector<Vector3i> otherPiece(Level &level, const Vector3i &position, const BlockState &state);

    static void breakOtherHalf(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

private:
    static int headFace(const BlockState &state);
};

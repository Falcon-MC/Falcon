#pragma once

#include "Block/BlockState.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3i.h"

#include <string>
#include <vector>

class Level;
class ServerNetworkHandler;

/**
 * Piston extension and retraction. A move removes the pushed blocks at once and places
 * them when the arm animation finishes in tick().
 */
class PistonSystem {
public:
    static const int MOVE_BLOCK_LIMIT = 12;

    static bool isPiston(const std::string &identifier);

    static bool isSticky(const std::string &identifier);

    static bool isArmCollision(const std::string &identifier);

    static int getPistonFace(const BlockState &state);

    static int getStoredFacing(const BlockState &state);

    static bool isExtended(Level &level, const Vector3i &position, const BlockState &state);

    static bool isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    static void tick(ServerNetworkHandler &owner, Level &level);

    static bool isMovingNear(const Level &level, const AxisAlignedBB &area);

    static void onBlockBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state);

    static bool canBePushed(const BlockState &state);

    static bool canBePulled(const BlockState &state);

    static bool breaksWhenMoved(const BlockState &state);

    static bool sticksToPiston(const BlockState &state);

    static bool canSticksBlock(const BlockState &state);

private:
    static bool _checkState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state, bool powered);

    static bool _doMove(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                        bool extending);
};

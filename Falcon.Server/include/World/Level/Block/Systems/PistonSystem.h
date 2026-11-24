#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

class Level;
class ServerNetworkHandler;

// pushed blocks are removed at once, placed back when the arm animation ends in tick()
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

    // every move of `level`, or only those touching `columns` (packed chunk x << 32 | z)
    static void finishMoves(ServerNetworkHandler &owner, Level &level,
                            const std::unordered_set<int64_t> *columns = nullptr);

    static bool isMovingInto(const Level &level, const Vector3i &position);

    // the level is going away: moves its last updates started are dropped with it
    static void forgetLevel(const Level &level);

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

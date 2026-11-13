#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Actor;
class BlockState;
class Level;
class ServerNetworkHandler;

// trip wires and target blocks: the two detectors RedstoneSystem asks for power
class DetectorSystem {
public:
    // cells a hook scans for the facing hook: 40 wires between them at most
    static constexpr int32_t MAX_HOOK_REACH = 41;
    // a tripped wire's scheduled recheck
    static constexpr int32_t WIRE_RECHECK_TICKS = 10;
    // a target's scheduled reset: arrows and tridents, then other projectiles; TargetBlock
    static constexpr int32_t TARGET_ARROW_TICKS = 20;
    static constexpr int32_t TARGET_OTHER_TICKS = 8;

    static bool isHook(const BlockState &state);

    static bool isWire(const BlockState &state);

    static bool isTarget(const BlockState &state);

    // the face a hook looks out of towards its wire, a RedstoneFace value
    static int hookFacing(const BlockState &state);

    // what a hook or a target gives a neighbour; -1 for any other block
    static int weakPower(Level &level, const Vector3i &position, const BlockState &state, int face);

    static int strongPower(Level &level, const Vector3i &position, const BlockState &state, int face);

    // entities stepping on wires and target expiry; from RedstoneSystem::tick
    static void tick(ServerNetworkHandler &owner, Level &level);

    // a wire's scheduled recheck
    static void onWireScheduled(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state);

    // a projectile struck a target; at: where it was, previous: where it came from
    static void onTargetHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const Vector3f &at,
                            const Vector3f &previous, Actor &projectile, bool arrowLike);

    // the power a hit at a point on the block's surface gives: 15 at a face centre, 1 at its corners
    static int targetPowerFor(const Vector3i &position, const Vector3f &hit);

    // where the segment from previous to at enters the block; at when it never crosses it
    static Vector3f targetHitPoint(const Vector3i &position, const Vector3f &at, const Vector3f &previous);
};

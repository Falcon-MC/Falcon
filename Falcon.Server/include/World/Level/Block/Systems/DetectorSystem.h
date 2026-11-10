#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Actor;
class BlockState;
class Level;
class ServerNetworkHandler;

// trip wires and target blocks: the two detectors RedstoneSystem asks for power
// TODO: every delay, range and formula here follows the documented behaviour, none is verified in the game
class DetectorSystem {
public:
    // wires a hook reaches along its direction
    static constexpr int32_t MAX_WIRE_LENGTH = 40;
    // a tripped wire rechecks for entities after this many ticks
    static constexpr int32_t WIRE_RECHECK_TICKS = 10;
    // a target stays powered this long: arrows and tridents, then other projectiles
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

    // entities on wires, wire rechecks and target expiry; from RedstoneSystem::tick
    static void tick(ServerNetworkHandler &owner, Level &level);

    // a projectile struck a target; at: where it was, previous: where it came from
    static void onTargetHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const Vector3f &at,
                            const Vector3f &previous, Actor &projectile, bool arrowLike);

    // the power a hit at a face point gives: 15 at the centre, 1 at the rim
    static int targetPowerFor(const Vector3i &position, const Vector3f &at, const Vector3f &previous);
};

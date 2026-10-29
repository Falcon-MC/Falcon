#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class NetworkIdentifier;
class PlayerAuthInputPacket;
class ServerNetworkHandler;
class ServerPlayer;

/**
 * Server-side replay of player travel physics for each PlayerAuthInput. The
 * simulation property enables replay; the strict movement property chooses the BDS
 * position reconciliation policy. Velocity always comes from server physics.
 */
class PlayerMovementSimulator {
public:
    static constexpr float PLAYER_BASE_OFFSET = 1.62f;

    static float groundAcceleration(const ServerPlayer &player, float friction, bool sprinting);

    static void apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                      const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting);
};

#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class NetworkIdentifier;
class PlayerAuthInputPacket;
class ServerNetworkHandler;
class ServerPlayer;

/**
 * Server-side replay of the client's walking physics for each PlayerAuthInput. When the
 * client's reported position or velocity strays past the configured threshold, the
 * simulated result wins and a movement correction is sent.
 */
class PlayerMovementSimulator {
public:
    static constexpr float PLAYER_BASE_OFFSET = 1.62f;

    static void apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                      const PlayerAuthInputPacket &packet, Vector3f &feetPosition);
};

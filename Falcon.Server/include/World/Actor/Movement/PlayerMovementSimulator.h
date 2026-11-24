#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/Movement/MovementConstants.h"

#include <cstdint>

class NetworkIdentifier;
class PlayerAuthInputPacket;
class ServerNetworkHandler;
class ServerPlayer;

// replays travel physics per PlayerAuthInput; the simulation property enables it, the strict movement
// property picks the position reconciliation policy; velocity always comes from server physics
class PlayerMovementSimulator {
public:
    static constexpr float PLAYER_BASE_OFFSET = MovementPhysics::PLAYER_BASE_OFFSET;

    static float groundAcceleration(const ServerPlayer &player, float friction, bool sprinting);

    static void apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                      const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting);
};

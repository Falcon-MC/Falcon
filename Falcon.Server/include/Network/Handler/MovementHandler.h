#pragma once

#include "Core/Math/Vector3f.h"

class Level;
class ServerNetworkHandler;
class ServerPlayer;
class NetworkIdentifier;
class PlayerAuthInputPacket;

class MovementHandler {
public:
    //TODO: MORE INFORMATIONS
    static void handlePlayerAuthInput(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                      const PlayerAuthInputPacket &packet);

    static void handleMovement(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &feetPosition,
                               const Vector3f &rotation);

    static bool checkGroundState(Level &level, const Vector3f &feetPosition);

    static void tickFluidEffects(ServerNetworkHandler &owner, ServerPlayer &player);
};

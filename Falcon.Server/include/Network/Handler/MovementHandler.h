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

    /**
     * Drains or refills the player's air: it drops by one each tick with the eyes in water and hurts every 20 ticks
     * once empty, a turtle helmet grants 200 ticks of breath after surfacing, and water breathing or conduit power
     * stop it from dropping at all. Creative and spectator players never lose air.
     */
    static void tickBreathing(ServerNetworkHandler &owner, ServerPlayer &player, bool eyeInWater);
};

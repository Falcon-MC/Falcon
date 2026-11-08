#pragma once

#include "Core/Math/Vector3f.h"

class Actor;
class Level;
class ServerNetworkHandler;
class ServerPlayer;
class NetworkIdentifier;
class PlayerAuthInputPacket;

class MovementHandler {
public:
    static void handlePlayerAuthInput(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                      const PlayerAuthInputPacket &packet);

    static bool requestFlight(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player, bool flying,
                              bool deferUntilInput = false);

    static void handleMovement(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &feetPosition,
                               const Vector3f &rotation);

    static bool checkGroundState(Level &level, const Vector3f &feetPosition, const Actor *actor = nullptr);

    static bool checkGroundState(Level &level, const Vector3f &previousFeet, const Vector3f &feetPosition,
                                 const Actor *actor = nullptr);

    static void tickFluidEffects(ServerNetworkHandler &owner, ServerPlayer &player);

    // one air per tick with eyes in water, a drowning hit every 20 ticks once empty; turtle helmet: 200 ticks
    // of breath after diving in; water breathing or conduit power stop the drop; creative/spectator never lose air
    static void tickBreathing(ServerNetworkHandler &owner, ServerPlayer &player, bool eyeInWater);
};

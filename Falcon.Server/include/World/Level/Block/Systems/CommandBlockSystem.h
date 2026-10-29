#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

class Level;
class ServerNetworkHandler;
class ServerPlayer;
class CommandBlockActor;
class CommandBlockUpdatePacket;

class CommandBlockSystem {
public:
    static int64_t packPosition(const Vector3i &position);

    static CommandBlockActor *find(Level &level, const Vector3i &position);

    static CommandBlockActor &getOrCreate(Level &level, const Vector3i &position);

    static void remove(Level &level, const Vector3i &position);

    static void onCommandBlockUpdate(ServerNetworkHandler &owner, ServerPlayer &player,
                                     const CommandBlockUpdatePacket &packet);

    static void setPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool powered);

    static void trigger(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int chain);

    static void broadcastData(ServerNetworkHandler &owner, Level &level, const CommandBlockActor &actor);

    static void tickCommandBlocks(ServerNetworkHandler &owner, Level &level);
};

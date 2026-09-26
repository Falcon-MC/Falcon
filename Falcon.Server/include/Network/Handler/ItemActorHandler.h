#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <vector>

class ServerNetworkHandler;
class ServerPlayer;
class ItemActor;
class ItemStack;
class Level;

class ItemActorHandler {
public:
    static const int DROP_PICKUP_DELAY = 10;
    static const int DEATH_DROP_PICKUP_DELAY = 40;

    static Vector3f randomDropMotion();

    static Vector3f randomDropAroundMotion();

    static ItemActor *dropItem(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                               const ItemStack &item, const Vector3f &motion, int pickupDelay);

    static void sendItemActorsTo(ServerNetworkHandler &owner, ServerPlayer &player);

    static void tickItemActors(ServerNetworkHandler &owner);

    static ItemActor *restoreItem(ServerNetworkHandler &owner, Level &level, const Tag &data);

    static void saveItemsInChunk(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ,
                                 std::vector<Tag> &out, bool cull);
};

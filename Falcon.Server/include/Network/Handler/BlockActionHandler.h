#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

class ServerNetworkHandler;
class ServerPlayer;
class ItemStack;
class ItemUseTransaction;
class Level;
class Packet;
class BlockState;
class BlockActor;

class BlockActionHandler {
public:
    static void broadcastToViewers(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                   const Packet &packet, const ServerPlayer *except = nullptr);

    static void broadcastBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state, uint32_t layer = 0);

    static void broadcastBlockActorData(ServerNetworkHandler &owner, Level &level, const BlockActor &blockActor);

    static int32_t breakSpeedEventData(double speed);

    static void breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    static void destroyBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &brokenState, bool dropItems, const ItemStack &tool);

    static void spawnBlockDrops(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &brokenState, const ItemStack &tool);

    static void startBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                   int32_t face);

    static void continueBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player);

    static void stopBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player);

    static void completeBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                      bool clientPredicted = true);

    static void sendBreakingFx(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool canInteractWithBlock(ServerPlayer &player, const Vector3i &position);

    static void placeBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemUseTransaction &transaction);

private:
    static constexpr int32_t NETHER_PLACEMENT_LIMIT = 127;

    static constexpr float MAX_SYNC_DISTANCE_SQUARED = 10000.0f;

    static bool interactBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                              const ItemUseTransaction &transaction, bool selectedSlotChanged);

    static bool matchesTransactionItem(const ItemStack &held, const ItemStack &sent);

    static bool isBlockChangeAllowed(Level &level, const Vector3i &position, const ServerPlayer &player);

    static bool isBlockedByActor(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, const ServerPlayer &placer);
};

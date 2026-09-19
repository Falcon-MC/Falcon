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

class BlockActionHandler {
public:
    static void broadcastToViewers(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                   const Packet &packet);

    static void broadcastBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state);

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
};

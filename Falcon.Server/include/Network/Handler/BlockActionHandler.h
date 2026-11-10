#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

class ServerNetworkHandler;
class ServerPlayer;
class Actor;
class ItemStack;
class ItemUseTransaction;
class Level;
class Packet;
class BlockState;
class BlockActor;
class PacketCodecContext;
class PlayerAuthInputPacket;

class BlockActionHandler {
public:
    // why a server-authoritative break was refused, for diagnostics plugins
    struct BreakRejection {
        const char *mReason = "";
        std::string mBlock;
        bool mBreaking = false;
        Vector3i mBreakingPosition;
        int64_t mElapsedTicks = 0;
        int64_t mExpectedTicks = 0;
        double mProgress = 0.0;
        double mSpeed = 0.0;
    };
    using BreakRejectionHook = void (*)(ServerPlayer &player, const Vector3i &position,
                                        const BreakRejection &rejection);

    static void setBreakRejectionHook(BreakRejectionHook hook);

    static void broadcastToViewers(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                   const Packet &packet, const ServerPlayer *except = nullptr);

    static void broadcastBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state, uint32_t layer = 0);

    static void broadcastBlockActorData(ServerNetworkHandler &owner, Level &level, const BlockActor &blockActor);

    static int32_t breakSpeedEventData(double speed);

    static void breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    // entitySource: who broke it, for the custom block onBreak hook
    static void destroyBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &brokenState, bool dropItems, const ItemStack &tool,
                             const Actor *entitySource = nullptr);

    static void spawnBlockDrops(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &brokenState, const ItemStack &tool);

    static void startBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                   int32_t face);

    static void continueBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player);

    static void stopBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player);

    static void completeBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    // resends blocks an unprocessed input predicted to break or place: the client drops its prediction
    static void resyncDroppedInput(ServerNetworkHandler &owner, ServerPlayer &player,
                                   const PlayerAuthInputPacket &packet);

    static void sendBreakingFx(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool canInteractWithBlock(ServerPlayer &player, const Vector3i &position);

    static bool canBreakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    static void placeBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemUseTransaction &transaction);

private:
    static bool _canInteractWithBlock(ServerPlayer &player, const Vector3i &position, float rangeScalar);

    static constexpr int32_t NETHER_PLACEMENT_LIMIT = 127;

    static constexpr float MAX_SYNC_DISTANCE_SQUARED = 10000.0f;

    static bool interactBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemUseTransaction &transaction,
                              bool selectedSlotChanged);

    // compares as the client sees them: an item it only knows under another one still matches
    static bool matchesTransactionItem(const PacketCodecContext &context, const ItemStack &held, const ItemStack &sent);

    static bool isBlockChangeAllowed(Level &level, const Vector3i &position, const ServerPlayer &player);

    static bool isBlockedByActor(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, const ServerPlayer &placer);
};

#include "Network/Handler/BadPacketHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

namespace {
    const int MINIMUM_CHUNK_RADIUS = 1;
    const int MAXIMUM_CHUNK_RADIUS = 96;

    bool validMoveComponent(float value) {
        return std::isfinite(value) && value >= -1.001f && value <= 1.001f;
    }

    bool validFace(int32_t face) {
        return face >= 0 && face <= 5;
    }

    bool hotbarSlot(int32_t slot) {
        return slot >= 0 && slot < 9;
    }
}

bool BadPacketHandler::inspect(ServerPlayer &player, const PlayerAuthInputPacket &packet, std::string &outReason) {
    const int64_t tick = packet.mTick;
    if (tick == 0 && player.hasSeenNonZeroClientTick()) {
        outReason = "input tick returned to zero";
        return true;
    }
    if (tick != 0)
        player.markNonZeroClientTickSeen();

    const bool jumpBoost = player.hasEffect(MobEffectId::JumpBoost);
    const bool validVerticalMotion = jumpBoost
                                     ? std::isfinite(packet.mMotionY)
                                     : validMoveComponent(packet.mMotionY);
    if (!validMoveComponent(packet.mMotionX) || !validVerticalMotion) {
        outReason = "invalid move vector";
        return true;
    }

    const double positionY = (double) packet.mPosition.y;
    const double positionX = (double) packet.mPosition.x;
    const double positionZ = (double) packet.mPosition.z;

    if (!std::isfinite(positionX) || !std::isfinite(positionY) || !std::isfinite(positionZ)) {
        outReason = "invalid position";
        return true;
    }

    if (packet.mHasItemUseTransaction) {
        if (packet.mItemUseTransaction.mActionType == 0 && !validFace(packet.mItemUseTransaction.mBlockFace)) {
            outReason = "invalid block face";
            return true;
        }
    }

    for (const PlayerBlockActionData &action: packet.mPlayerActions) {
        if (action.mAction == PlayerActionType::DimensionChangeRequestOrCreativeDestroyBlock &&
            player.getGameType() != (int32_t) GameType::Creative) {
            outReason = "creative destroy outside creative";
            return true;
        }

        if (action.mAction != PlayerActionType::AbortBreak && action.mAction != PlayerActionType::StopBreak &&
            !validFace(action.mFace)) {
            outReason = "invalid block action face";
            return true;
        }
    }

    return false;
}

bool BadPacketHandler::inspect(ServerPlayer &player, const InventoryTransactionPacket &packet,
                             std::string &outReason) {
    if (packet.mTransactionType == InventoryTransactionType::ItemUseOnEntity) {
        if (!hotbarSlot(packet.mHotbarSlot)) {
            outReason = "invalid hotbar slot";
            return true;
        }

        if (packet.mActionType == 1 && packet.mRuntimeActorId == (int64_t) player.getRuntimeId()) {
            outReason = "self attack";
            return true;
        }
    } else if (packet.mTransactionType == InventoryTransactionType::ItemUse) {
        if (!hotbarSlot(packet.mHotbarSlot)) {
            outReason = "invalid hotbar slot";
            return true;
        }

        if (packet.mActionType != 1 && !validFace(packet.mBlockFace)) {
            outReason = "invalid block face";
            return true;
        }
    } else if (packet.mTransactionType == InventoryTransactionType::ItemRelease) {
        if (!hotbarSlot(packet.mHotbarSlot)) {
            outReason = "invalid hotbar slot";
            return true;
        }
    }

    return false;
}

bool BadPacketHandler::inspect(ServerPlayer &player, const ItemStackRequestPacket &packet, std::string &outReason) {
    for (const ItemStackRequest &request: packet.mRequests) {
        for (const ItemStackRequestAction &action: request.mActions) {
            if (action.mType == ItemStackRequestActionType::CraftCreative &&
                player.getGameType() != (int32_t) GameType::Creative) {
                outReason = "creative craft outside creative";
                return true;
            }
        }
    }

    return false;
}

bool BadPacketHandler::inspect(ServerPlayer &, const MobEquipmentPacket &packet, std::string &outReason) {
    if (!hotbarSlot(packet.mHotbarSlot)) {
        outReason = "invalid hotbar slot";
        return true;
    }

    return false;
}

bool BadPacketHandler::inspect(ServerPlayer &, const RequestChunkRadiusPacket &packet, std::string &outReason) {
    if (packet.mRadius < MINIMUM_CHUNK_RADIUS || packet.mRadius > MAXIMUM_CHUNK_RADIUS) {
        outReason = "invalid chunk radius";
        return true;
    }

    return false;
}

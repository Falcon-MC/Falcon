#pragma once

#include "Core/Math/Vector3i.h"
#include "Level/BlockState.h"

#include <cstdint>

class ItemStack;
class ItemUseTransaction;
class Level;
class ServerNetworkHandler;
class ServerPlayer;

class BucketItem {
public:
    enum class Content {
        Empty,
        Water,
        Lava,
        PowderSnow,
        None
    };

    static Content getContent(const ItemStack &item);

    static bool isBucket(const ItemStack &item);

    static bool use(ServerNetworkHandler &owner, ServerPlayer &player, const ItemUseTransaction &transaction);

    static bool applyResult(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &heldItem,
                            const char *resultIdentifier);

private:
    static bool isReplaceable(const BlockState &state);

    static Vector3i getPlacementPosition(const ItemUseTransaction &transaction, const BlockState &clickedState);

    static BlockState makeLiquidState(Content content);

    static const char *getFilledIdentifier(Content content);

    static void sendBlockState(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static void sendBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state, uint32_t layer = 0);

    static bool isWaterloggable(Level &level, const Vector3i &position);

    static void sendSound(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const char *sound);

    static void sendArmSwing(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);
};

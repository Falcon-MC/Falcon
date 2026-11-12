#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class ItemStack;
class Level;
class ServerNetworkHandler;
class ServerPlayer;
class SignBlockActor;
class Tag;

// sign interaction, editor lock and text updates shared by the standing, wall and hanging signs
class SignEditing {
public:
    enum class Action { None, WaxedFail, Dye, GlowInkSac, InkSac, Honeycomb, Edit };

    static constexpr const char *WAXED_FAIL_SOUND = "block.sign.waxed_interact_fail";
    static constexpr int32_t DYE_EVENT = 1065;
    static constexpr int32_t INK_SAC_EVENT = 1066;
    static constexpr int32_t WAX_ON_EVENT = 2030;

    static bool interact(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state);

    static void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const ItemStack &usedItem);

    static bool openEditor(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, bool front);

    static bool isFrontFacing(const BlockState &state, const Vector3i &position, float playerX, float playerZ);

    static float rotationOf(const BlockState &state);

    static Action decide(ServerNetworkHandler &owner, SignBlockActor &sign, ServerPlayer &player, bool front,
                         const ItemStack &held);

    // the text color a dye gives, 0xAARRGGBB
    static int32_t dyeTextColor(uint8_t itemColor);

    // takes the text of each side present in the update, only from the player holding the editor
    static bool applyUpdate(ServerNetworkHandler &owner, ServerPlayer &player, SignBlockActor &sign, const Tag &update);

private:
    static bool editorStillHolds(ServerNetworkHandler &owner, SignBlockActor &sign);
};

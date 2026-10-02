#include "Block/Blocks/EndPortalFrameBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockState.h"
#include "Block/Blocks/PortalHelpers.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <string>

FALCON_REGISTER_BLOCK(EndPortalFrameBlock, 7);

using namespace PortalHelpers;

namespace {
    const char *ENDER_EYE_IDENTIFIER = "minecraft:ender_eye";

    int32_t stateFlag(const BlockState &state, const std::string &key) {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr)
            return 0;

        if (tag->getType() == Tag::Type::Byte)
            return tag->asByte();

        if (tag->getType() == Tag::Type::Int)
            return tag->asInt();

        return 0;
    }

    const char *expectedFrameFacing(int32_t x, int32_t z) {
        if (x == -2)
            return "east";

        if (x == 2)
            return "west";

        if (z == -2)
            return "south";

        return "north";
    }
}

bool EndPortalFrameBlock::matches(const std::string &identifier) {
    return identifier == END_PORTAL_FRAME_IDENTIFIER;
}

bool EndPortalFrameBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                     const BlockState &state) const {
    PlayerInventory &inventory = player.getInventory();
    const ItemStack &held = inventory.getItemInHand();

    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    if (held.mDefinition->getIdentifier() != ENDER_EYE_IDENTIFIER)
        return false;

    if (stateFlag(state, "end_portal_eye_bit") != 0)
        return false;

    Level &level = owner.getLevelFor(player);
    if (!isInsideLevel(level, position))
        return false;

    Tag states = state.mStates;
    states.putByte("end_portal_eye_bit", 1);

    const BlockState filled(state.mName, states);
    writeBlock(level, position, filled, &owner);

    owner.playNamedSound(level, PlaySoundName::END_PORTAL_FRAME_FILL, centerOf(position), 1.0f, 1.0f);

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack updated = inventory.getItemInHand();
        updated.mCount -= 1;

        if (updated.mCount <= 0)
            inventory.setItemInHand(ItemStack::air());
        else
            inventory.setItemInHand(std::move(updated));

        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    tryCompletePortal(level, position, &owner);
    return true;
}

bool EndPortalFrameBlock::tryCompletePortal(Level &level, const Vector3i &framePosition,
                                            ServerNetworkHandler *owner) {
    int32_t minX = 0;
    int32_t minZ = 0;
    bool hasFrame = false;

    for (int32_t x = -4; x <= 4; ++x) {
        for (int32_t z = -4; z <= 4; ++z) {
            if (identifierAt(level, framePosition.x + x, framePosition.y, framePosition.z + z)
                != END_PORTAL_FRAME_IDENTIFIER)
                continue;

            if (!hasFrame) {
                hasFrame = true;
                minX = framePosition.x + x;
                minZ = framePosition.z + z;
                continue;
            }

            minX = std::min(minX, framePosition.x + x);
            minZ = std::min(minZ, framePosition.z + z);
        }
    }

    if (!hasFrame)
        return false;

    const Vector3i center(minX + 2, framePosition.y, minZ + 2);

    for (int32_t x = -2; x <= 2; ++x) {
        for (int32_t z = -2; z <= 2; ++z) {
            if ((x == -2 || x == 2) && (z == -2 || z == 2))
                continue;

            if (x != -2 && x != 2 && z != -2 && z != 2)
                continue;

            const Vector3i target(center.x + x, center.y, center.z + z);
            if (!isInsideLevel(level, target))
                return false;

            const BlockState state = level.getBlockState(target.x, target.y, target.z);
            if (state.mName != END_PORTAL_FRAME_IDENTIFIER)
                return false;

            if (stateFlag(state, "end_portal_eye_bit") == 0)
                return false;

            if (stateText(state, "minecraft:cardinal_direction") != expectedFrameFacing(x, z))
                return false;
        }
    }

    const BlockState endPortal((std::string(END_PORTAL_IDENTIFIER)));

    for (int32_t x = -1; x <= 1; ++x) {
        for (int32_t z = -1; z <= 1; ++z) {
            writeBlock(level, Vector3i(center.x + x, center.y, center.z + z), endPortal, owner);
        }
    }

    if (owner != nullptr)
        owner->playNamedSound(level, PlaySoundName::END_PORTAL_SPAWN, centerOf(center), 1.0f, 1.0f);

    return true;
}

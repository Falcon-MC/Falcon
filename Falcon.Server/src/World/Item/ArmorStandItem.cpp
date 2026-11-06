#include "World/Item/ArmorStandItem.h"

#include "World/Actor/ActorClassRegistry.h"
#include "Core/Math/AxisAlignedBB.h"
#include "World/Actor/Misc/ArmorStandActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

FALCON_REGISTER_ITEM(ArmorStandItem, 110);

namespace {
    const float ROTATION_STEP_DEGREES = 45.0f;
    const float HALF_TURN_DEGREES = 180.0f;
    const float FULL_TURN_DEGREES = 360.0f;
    const float HALF_STEP_DEGREES = 22.5f;
    const BlockState AIR("minecraft:air");

    AxisAlignedBB actorBox(const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half,
                             position.x + half, position.y + size.mHeight, position.z + half);
    }

    // the game tests the two whole blocks the stand stands in, against every actor
    bool isAreaFree(ServerNetworkHandler &owner, Level &level, const Vector3i &target) {
        const AxisAlignedBB area((float) target.x, (float) target.y, (float) target.z,
                                 (float) target.x + 1.0f, (float) target.y + 2.0f, (float) target.z + 1.0f);

        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &other = entry.second;
            if (other.isSpawned() && &owner.getLevelFor(other) == &level
                && area.intersectsWith(actorBox(other.getPosition(), ActorClassRegistry::getSize("minecraft:player"))))
                return false;
        }

        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;
            if (!actor.isDead() && !actor.isExpired() && &owner.getLevelFor(actor) == &level
                && area.intersectsWith(actorBox(actor.getPosition(), actor.getSize())))
                return false;
        }

        return true;
    }

    Vector3i offsetByFace(const Vector3i &position, int32_t face) {
        switch (face) {
            case 2:
                return Vector3i(position.x, position.y, position.z - 1);
            case 3:
                return Vector3i(position.x, position.y, position.z + 1);
            case 4:
                return Vector3i(position.x - 1, position.y, position.z);
            case 5:
                return Vector3i(position.x + 1, position.y, position.z);
            default:
                return Vector3i(position.x, position.y + 1, position.z);
        }
    }

    void clearForStand(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (state.mName == AIR.mName)
            return;

        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (block != nullptr && block->staysUnderPlacedActor())
            return;

        level.setBlock(position, AIR, true);
    }

}

float ArmorStandItem::placedYaw(float playerYaw) {
    float yaw = std::fmod(playerYaw - HALF_TURN_DEGREES + HALF_TURN_DEGREES, FULL_TURN_DEGREES);
    if (yaw < 0.0f)
        yaw += FULL_TURN_DEGREES;
    return std::floor((yaw - HALF_TURN_DEGREES + HALF_STEP_DEGREES) / ROTATION_STEP_DEGREES) * ROTATION_STEP_DEGREES;
}

ArmorStandItem::ArmorStandItem(const Item &base) : Item(base) {}

bool ArmorStandItem::matches(const std::string &identifier) {
    return identifier == ArmorStandActor::IDENTIFIER;
}

bool ArmorStandItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                  const Vector3i &blockPosition, int32_t face,
                                  const Vector3f &clickPosition) const {
    (void) clickPosition;

    if (face == 0)
        return false;

    const int32_t gameType = player.getGameType();
    if (gameType == (int32_t) GameType::Spectator || gameType == (int32_t) GameType::Adventure)
        return false;

    Level &level = owner.getLevelFor(player);
    const bool replacesClicked = BlockSupport::isReplaceable(
            level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z));
    const Vector3i target = replacesClicked ? blockPosition : offsetByFace(blockPosition, face);
    const Vector3i above(target.x, target.y + 1, target.z);
    if (above.y > level.getMaxY())
        return false;

    if (!BlockSupport::isReplaceable(level.getBlockState(target.x, target.y, target.z))
        || !BlockSupport::isReplaceable(level.getBlockState(above.x, above.y, above.z)))
        return false;

    if (!isAreaFree(owner, level, target))
        return false;

    clearForStand(level, target);
    clearForStand(level, above);

    const Vector3f spawnPosition((float) target.x + 0.5f, (float) target.y, (float) target.z + 0.5f);
    const float yaw = placedYaw(player.getRotation().y);
    ServerActor *stand = owner.spawnActor(level, ArmorStandActor::IDENTIFIER, spawnPosition,
                                          [&](ServerActor &actor) {
                                              actor.setRotation(Vector3f(0.0f, yaw, yaw));
                                          });
    if (stand == nullptr)
        return false;

    owner.broadcastLevelEvent(level, ArmorStandActor::LEVEL_EVENT_PLACE, spawnPosition, 0);

    if (gameType != (int32_t) GameType::Creative) {
        ItemStack remaining = item;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();

        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    return true;
}

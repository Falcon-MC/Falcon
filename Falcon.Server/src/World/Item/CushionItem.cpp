#include "World/Item/CushionItem.h"

#include "World/Actor/Misc/CushionActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "Core/Color/DyeColor.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>

FALCON_REGISTER_ITEM(CushionItem, 110);

namespace {
    // another cushion under the clicked point blocks the placement: the game fetches cushions in a box 0.001 high
    // around it (sub_147B980C0)
    bool isOccupied(ServerNetworkHandler &owner, Level &level, const Vector3f &point) {
        const AxisAlignedBB probe(point.x, point.y - 0.001f, point.z, point.x, point.y + 0.001f, point.z);
        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;
            if (actor.isDead() || actor.getTypeId() != CushionActor::IDENTIFIER || !actor.isIn(level))
                continue;

            const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(actor);
            if (box.mMinX <= probe.mMaxX && box.mMaxX >= probe.mMinX && box.mMinY <= probe.mMaxY
                && box.mMaxY >= probe.mMinY && box.mMinZ <= probe.mMaxZ && box.mMaxZ >= probe.mMinZ)
                return true;
        }

        return false;
    }

    // the player's yaw snapped to the nearest quarter turn, as the game rotates a placed cushion
    float placedYaw(float yaw) {
        float turned = std::fmod(yaw, 360.0f);
        if (turned < 0.0f)
            turned += 360.0f;
        return std::floor((turned - 180.0f + 45.0f) / 90.0f) * 90.0f;
    }
}

CushionItem::CushionItem(const Item &base) : Item(base) {}

bool CushionItem::matches(const std::string &identifier) {
    return DyeColor::fromIdentifier(identifier, SUFFIX) != DyeColor::INVALID;
}

bool CushionItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                               const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    if (player.getGameType() == (int32_t) GameType::Spectator)
        return false;

    const uint8_t color = DyeColor::fromIdentifier(getIdentifier(), SUFFIX);
    if (color == DyeColor::INVALID)
        return false;

    // top faces only; the cushion sits where the face was clicked: a snow layer keeps it low (sub_147B980C0)
    if (face != 1 || !std::isfinite(clickPosition.x) || !std::isfinite(clickPosition.y)
        || !std::isfinite(clickPosition.z))
        return false;

    Level &level = owner.getLevelFor(player);
    // the click is client input: kept on the clicked block, or the occupancy probe misses its cushion
    const Vector3f clicked((float) blockPosition.x + std::max(0.0f, std::min(1.0f, clickPosition.x)),
                           (float) blockPosition.y + std::max(0.0f, std::min(1.0f, clickPosition.y)),
                           (float) blockPosition.z + std::max(0.0f, std::min(1.0f, clickPosition.z)));
    if (isOccupied(owner, level, clicked))
        return false;

    const Vector3f spawnPosition((float) blockPosition.x + 0.5f, clicked.y, (float) blockPosition.z + 0.5f);
    const float yaw = placedYaw(player.getRotation().y);

    // color before the spawn packet: a later variant change makes the client rebuild the model for a few frames
    ServerActor *spawned = owner.spawnActor(level, CushionActor::IDENTIFIER, spawnPosition,
                                            [yaw, color](ServerActor &cushion) {
                                                cushion.setRotation(Vector3f(0.0f, yaw, yaw));
                                                static_cast<CushionActor &>(cushion).setColor(color);
                                            });
    if (spawned == nullptr)
        return false;

    owner.playLevelSound(level, LevelSoundEvent::SPAWN, spawnPosition, CushionActor::IDENTIFIER, -1);

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack remaining = item;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();

        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }

    return true;
}

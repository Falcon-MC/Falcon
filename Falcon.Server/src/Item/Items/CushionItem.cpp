#include "Item/Items/CushionItem.h"

#include "Actor/Misc/CushionActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockSupport.h"
#include "Core/Color/DyeColor.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/ItemClassRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

FALCON_REGISTER_ITEM(CushionItem, 110);

namespace {
    bool isOccupied(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;

            if (actor.isDead() || actor.getTypeId() != CushionActor::IDENTIFIER)
                continue;

            if (&owner.getLevelFor(actor) != &level)
                continue;

            const Vector3f actorPosition = actor.getPosition();
            if ((int32_t) std::floor(actorPosition.x) == position.x
                && (int32_t) std::floor(actorPosition.y) == position.y
                && (int32_t) std::floor(actorPosition.z) == position.z)
                return true;
        }

        return false;
    }
}

CushionItem::CushionItem(const Item &base) : Item(base) {}

bool CushionItem::matches(const std::string &identifier) {
    return DyeColor::fromIdentifier(identifier, SUFFIX) != DyeColor::INVALID;
}

bool CushionItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                               const Vector3i &blockPosition, int32_t face,
                               const Vector3f &clickPosition) const {
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Spectator)
        return false;

    const uint8_t color = DyeColor::fromIdentifier(getIdentifier(), SUFFIX);
    if (color == DyeColor::INVALID)
        return false;

    Level &level = owner.getLevelFor(player);
    const Vector3i target = BlockSupport::supportOf(blockPosition, face ^ 1);

    if (!BlockSupport::isReplaceable(level.getBlockState(target.x, target.y, target.z)))
        return false;

    const Vector3i below(target.x, target.y - 1, target.z);
    if (!BlockSupport::isSolidOrCauldron(level.getBlockState(below.x, below.y, below.z)))
        return false;

    if (isOccupied(owner, level, target))
        return false;

    const Vector3f spawnPosition((float) target.x + 0.5f, (float) target.y, (float) target.z + 0.5f);

    ServerActor *spawned = owner.spawnActor(level, CushionActor::IDENTIFIER, spawnPosition);
    if (spawned == nullptr)
        return false;

    static_cast<CushionActor *>(spawned)->setColor(color);

    EntityDataMap metadata;
    spawned->fillSpawnMetadata(metadata);
    owner.sendActorMetadata(*spawned, metadata);

    owner.playLevelSound(level, "spawn", spawnPosition, CushionActor::IDENTIFIER, -1);

    if (player.getGameType() != (int32_t) GameType::Creative) {
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

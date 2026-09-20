#include "Item/Items/EndCrystalItem.h"

#include "Actor/ActorSizeTable.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Actor/EndCrystalActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Item/ItemClassRegistry.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cstdlib>

FALCON_REGISTER_ITEM(EndCrystalItem, 110);

namespace {
    const float FULL_TURN_DEGREES = 360.0f;

    AxisAlignedBB crystalArea(const Vector3i &position) {
        return AxisAlignedBB((float) position.x, (float) position.y, (float) position.z,
                             (float) position.x + 1.0f, (float) position.y + 2.0f, (float) position.z + 1.0f);
    }

    AxisAlignedBB actorBox(const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half,
                             position.x + half, position.y + size.mHeight, position.z + half);
    }

    bool isAreaFree(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        const AxisAlignedBB area = crystalArea(position);

        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;

            if (!player.isSpawned() || &owner.getLevelFor(player) != &level)
                continue;

            if (area.intersectsWith(actorBox(player.getPosition(), ActorSizeTable::getSize("minecraft:player"))))
                return false;
        }

        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;

            if (actor.isDead() || &owner.getLevelFor(actor) != &level)
                continue;

            if (area.intersectsWith(actorBox(actor.getPosition(), ActorSizeTable::getSize(actor.getIdentifier()))))
                return false;
        }

        return true;
    }
}

EndCrystalItem::EndCrystalItem(const Item &base) : Item(base) {}

bool EndCrystalItem::matches(const std::string &identifier) {
    return identifier == "minecraft:end_crystal";
}

bool EndCrystalItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                  const Vector3i &blockPosition, int32_t face,
                                  const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Spectator)
        return false;

    Level &level = owner.getLevelFor(player);
    const std::string &base = level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z).mName;

    if (base != "minecraft:obsidian" && base != "minecraft:bedrock")
        return false;

    const Vector3i above(blockPosition.x, blockPosition.y + 1, blockPosition.z);
    if (above.y + 1 > level.getMaxY())
        return false;

    if (level.getBlockState(above.x, above.y, above.z).mName != "minecraft:air")
        return false;

    if (level.getBlockState(above.x, above.y + 1, above.z).mName != "minecraft:air")
        return false;

    if (!isAreaFree(owner, level, above))
        return false;

    const Vector3f spawnPosition((float) blockPosition.x + 0.5f, (float) above.y, (float) blockPosition.z + 0.5f);

    ServerActor *crystal = owner.spawnActor(level, EndCrystalActor::IDENTIFIER, spawnPosition);
    if (crystal == nullptr)
        return false;

    const float yaw = (float) std::rand() / (float) RAND_MAX * FULL_TURN_DEGREES;
    crystal->setRotation(Vector3f(0.0f, yaw, 0.0f));

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

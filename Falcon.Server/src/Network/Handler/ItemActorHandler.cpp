#include "Network/Handler/ItemActorHandler.h"

#include "Actor/ActorFlags.h"
#include "Actor/ItemActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/ItemData.h"
#include "Level/Level.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/AddItemActorPacket.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"
#include "Protocol/Packets/TakeItemActorPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <random>
#include <string>

namespace {
    std::mt19937 &dropRandom() {
        static std::mt19937 generator(0x5F3759DFu);
        return generator;
    }

    float randomUnitFloat() {
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        return distribution(dropRandom());
    }
}

Vector3f ItemActorHandler::randomDropMotion() {
    return Vector3f(randomUnitFloat() * 0.2f - 0.1f, 0.2f, randomUnitFloat() * 0.2f - 0.1f);
}

Vector3f ItemActorHandler::randomDropAroundMotion() {
    const float radius = randomUnitFloat() * 0.5f;
    const float angle = randomUnitFloat() * MathConstants::TWO_PI_F;

    return Vector3f(-std::sin(angle) * radius, 0.2f, std::cos(angle) * radius);
}

namespace {
    const float ITEM_GRAVITY = 0.04f;
    const float ITEM_HORIZONTAL_DRAG = 0.02f;
    const float ITEM_VERTICAL_DRAG = 0.02f;
    const float ITEM_GROUND_PROBE_DEPTH = 0.01f;
    const float ITEM_PICKUP_RANGE_SQUARED = 2.25f;

    bool isFireBlock(const std::string &identifier) {
        return identifier == "minecraft:fire" || identifier == "minecraft:soul_fire"
               || identifier == "minecraft:lava" || identifier == "minecraft:flowing_lava"
               || identifier == "minecraft:campfire" || identifier == "minecraft:soul_campfire";
    }

    bool isFireResistant(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return false;

        const FireResistantItemComponent *component = ItemDataTable::getComponents(
                item.mDefinition->getIdentifier()).get<FireResistantItemComponent>();
        return component != nullptr && component->isFireResistant();
    }

    bool isInFire(ServerNetworkHandler &owner, const ItemActor &actor) {
        const Vector3f &position = actor.getPosition();
        const int x = (int) std::floor(position.x);
        const int y = (int) std::floor(position.y);
        const int z = (int) std::floor(position.z);
        Level &level = owner.getLevelFor(actor);
        if (y < level.getMinY() || y > level.getMaxY())
            return false;
        return isFireBlock(level.getBlockState(x, y, z).mName);
    }

    int freeSpaceFor(const PlayerInventory &inventory, const ItemStack &item) {
        const int maxStackSize = PlayerInventory::getMaxStackSize(item);
        int free = 0;

        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; slot++) {
            const ItemStack &existing = inventory.getItem(slot);

            if (existing.isAir()) {
                free += maxStackSize;
                continue;
            }

            if (PlayerInventory::canStack(existing, item))
                free += maxStackSize - existing.mCount;
        }

        return free;
    }

    void spawnItemActorTo(ServerNetworkHandler &owner, ServerPlayer &player, const ItemActor &actor) {
        if (player.getDimension() != actor.getDimension())
            return;

        AddItemActorPacket add;
    add.mUniqueActorId = actor.getUniqueId();
    add.mRuntimeActorId = actor.getRuntimeId();
        add.mItemInHand = actor.getItem();
        add.mPosition = actor.getPosition();
        add.mMotion = actor.getMotion();
        add.mFromFishing = false;

        EntityDataEntry flags;
        flags.mId = ActorFlags::FLAGS_DATA_ID;
        flags.mFormat = EntityDataFormat::Long;
        flags.mLongValue = actor.getFlags().getLowBits();
        add.mMetadata.mEntries.push_back(flags);

        EntityDataEntry flags2;
        flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
        flags2.mFormat = EntityDataFormat::Long;
        flags2.mLongValue = actor.getFlags().getHighBits();
        add.mMetadata.mEntries.push_back(flags2);

        owner.getNetworkHandler().send(player.getNetworkIdentifier(), add, owner.getCodecContext());
    }

    void broadcastItemActorSpawn(ServerNetworkHandler &owner, const ItemActor &actor) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned())
                spawnItemActorTo(owner, entry.second, actor);
        }
    }

    void broadcastItemActorMove(ServerNetworkHandler &owner, const ItemActor &actor) {
        MoveActorAbsolutePacket move;
    move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
        move.mPosition = actor.getPosition();
        move.mRotation = actor.getRotation();
        move.mOnGround = actor.isOnGround();
        move.mTeleported = false;
        move.mForceMove = false;

        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getDimension() == actor.getDimension())
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), move, owner.getCodecContext());
        }
    }

    void broadcastItemActorRemove(ServerNetworkHandler &owner, const ItemActor &actor) {
        RemoveActorPacket remove;
    remove.mUniqueActorId = actor.getUniqueId();

        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getDimension() == actor.getDimension())
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), remove, owner.getCodecContext());
        }
    }

    bool hasSupportBelow(ServerNetworkHandler &owner, const ItemActor &actor) {
        const Vector3f &position = actor.getPosition();

        return owner.getLevelFor(actor).isSolidAt((int32_t) std::floor(position.x),
                                          (int32_t) std::floor(position.y - ITEM_GROUND_PROBE_DEPTH),
                                          (int32_t) std::floor(position.z));
    }

    void moveItemActor(ServerNetworkHandler &owner, ItemActor &actor) {
        if (actor.isOnGround()) {
            if (hasSupportBelow(owner, actor))
                return;

            actor.setOnGround(false);
        }

        Vector3f motion = actor.getMotion();
        const Vector3f position = actor.getPosition();

        motion.y -= ITEM_GRAVITY;

        Level &level = owner.getLevelFor(actor);
        Vector3f next = position;

        const int32_t currentY = (int32_t) std::floor(position.y);
        const int32_t currentZ = (int32_t) std::floor(position.z);

        if (motion.x != 0.0f) {
            const float candidateX = position.x + motion.x;
            if (level.isSolidAt((int32_t) std::floor(candidateX), currentY, currentZ))
                motion.x = 0.0f;
            else
                next.x = candidateX;
        }

        if (motion.z != 0.0f) {
            const float candidateZ = position.z + motion.z;
            if (level.isSolidAt((int32_t) std::floor(next.x), currentY, (int32_t) std::floor(candidateZ)))
                motion.z = 0.0f;
            else
                next.z = candidateZ;
        }

        next.y = position.y + motion.y;

        const int32_t blockX = (int32_t) std::floor(next.x);
        const int32_t blockZ = (int32_t) std::floor(next.z);
        const int32_t blockY = (int32_t) std::floor(next.y);

        if (motion.y < 0.0f && level.isSolidAt(blockX, blockY, blockZ)) {
            next.y = (float) (blockY + 1);
            motion = Vector3f(0.0f, 0.0f, 0.0f);
            actor.setOnGround(true);
        } else if (motion.y > 0.0f && level.isSolidAt(blockX, blockY, blockZ)) {
            next.y = position.y;
            motion.y = 0.0f;
        } else {
            motion.x *= 1.0f - ITEM_HORIZONTAL_DRAG;
            motion.y *= 1.0f - ITEM_VERTICAL_DRAG;
            motion.z *= 1.0f - ITEM_HORIZONTAL_DRAG;
        }

        actor.setPosition(next);
        actor.setMotion(motion);

        broadcastItemActorMove(owner, actor);
    }

    void tryPickupItemActor(ServerNetworkHandler &owner, ItemActor &actor) {
        if (actor.isRemoved() || !actor.canPickup())
            return;

        const Vector3f itemPosition = actor.getPosition();

        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;

            if (!player.isSpawned() || player.isDead())
                continue;

            if (player.getGameType() == (int32_t) GameType::Spectator)
                continue;

            if (player.getDimension() != actor.getDimension())
                continue;

            const Vector3f playerPosition = player.getPosition();
            const float deltaX = playerPosition.x - itemPosition.x;
            const float deltaY = playerPosition.y + 1.0f - itemPosition.y;
            const float deltaZ = playerPosition.z - itemPosition.z;

            if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > ITEM_PICKUP_RANGE_SQUARED)
                continue;

            PlayerInventory &inventory = player.getInventory();
            if (freeSpaceFor(inventory, actor.getItem()) < actor.getItem().mCount)
                continue;

            const ItemStack pickedItem = actor.getItem();

            std::vector<int> touchedSlots;
            if (!inventory.addItem(actor.getItem(), touchedSlots))
                continue;

            if (pickedItem.mDefinition != nullptr) {
                EntityItemPickupAfterEvent pickupEvent(player, pickedItem.mDefinition->getIdentifier(),
                                                       pickedItem.mCount);
                owner.getEventBus().after().mEntityItemPickup.emit(pickupEvent);
            }

            TakeItemActorPacket take;
    take.mItemRuntimeActorId = actor.getRuntimeId();
    take.mRuntimeActorId = player.getRuntimeId();

            for (auto &viewer: owner.getPlayers()) {
                if (viewer.second.isSpawned() && viewer.second.getDimension() == actor.getDimension())
                    owner.getNetworkHandler().send(viewer.second.getNetworkIdentifier(), take, owner.getCodecContext());
            }

            for (int slot: touchedSlots)
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

            actor.setRemoved(true);
            return;
        }
    }
}

ItemActor *ItemActorHandler::dropItem(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                      const ItemStack &item, const Vector3f &motion, int pickupDelay) {
    if (item.isAir() || item.mCount <= 0)
        return nullptr;

    std::unique_ptr<ItemActor> actor(new ItemActor(owner.allocateRuntimeId(), item));
    actor->setDimension(level.getDimensionType());
    actor->getItem().mUsingNetId = false;
    actor->getItem().mNetId = 0;
    actor->setPosition(position);
    actor->setMotion(motion);
    actor->setPickupDelay(pickupDelay);
    actor->resetFallDistance();

    ItemActor *result = actor.get();
    owner.getItemEntities().push_back(std::move(actor));

    broadcastItemActorSpawn(owner, *result);
    return result;
}

void ItemActorHandler::sendItemActorsTo(ServerNetworkHandler &owner, ServerPlayer &player) {
    for (const std::unique_ptr<ItemActor> &actor: owner.getItemEntities()) {
        if (!actor->isRemoved())
            spawnItemActorTo(owner, player, *actor);
    }
}

void ItemActorHandler::tickItemActors(ServerNetworkHandler &owner) {
    std::vector<std::unique_ptr<ItemActor>> &actors = owner.getItemEntities();

    for (auto it = actors.begin(); it != actors.end();) {
        ItemActor &actor = **it;

        actor.tick();
        const bool fireResistant = isFireResistant(actor.getItem());
        if (!fireResistant && isInFire(owner, actor))
            actor.setRemoved(true);

        if (!actor.isRemoved())
            moveItemActor(owner, actor);

        if (!fireResistant && !actor.isRemoved() && isInFire(owner, actor))
            actor.setRemoved(true);

        tryPickupItemActor(owner, actor);

        if (actor.isRemoved() || actor.isExpired()) {
            broadcastItemActorRemove(owner, actor);
            it = actors.erase(it);
            continue;
        }

        ++it;
    }
}

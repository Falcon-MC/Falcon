#include "Network/Handler/ItemActorHandler.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Block/FireBlock.h"
#include "World/Level/Block/LavaBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ItemData.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/AddItemActorPacket.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"
#include "Protocol/Packets/TakeItemActorPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
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

// Block::popResource: 0.15 to 0.85 inside the block on each axis (sub_1435AACD0)
Vector3f ItemActorHandler::randomPopPosition(const Vector3i &block) {
    const float x = randomUnitFloat() * 0.7f + 0.15f;
    const float y = randomUnitFloat() * 0.7f + 0.15f;
    const float z = randomUnitFloat() * 0.7f + 0.15f;
    return Vector3f((float) block.x + x, (float) block.y + y, (float) block.z + z);
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

    bool isCampfire(const std::string &identifier) {
        const ContainerBlockDefinition *definition = ContainerBlock::findDefinition(identifier);
        return definition != nullptr && definition->mKind == ContainerBlockKind::Campfire;
    }

    bool isFireBlock(const std::string &identifier) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        return dynamic_cast<const FireBlock *>(block) != nullptr || dynamic_cast<const LavaBlock *>(block) != nullptr
               || isCampfire(identifier);
    }

    bool isFireResistant(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return false;

        const FireResistantItemComponent *component =
                ItemDataTable::getComponents(item.mDefinition->getIdentifier()).get<FireResistantItemComponent>();
        return component != nullptr && component->isFireResistant();
    }

    bool isInFire(ServerNetworkHandler &owner, const ItemActor &actor) {
        const Vector3f &position = actor.getPosition();
        const int x = (int) std::floor(position.x);
        const int y = (int) std::floor(position.y);
        const int z = (int) std::floor(position.z);
        const BlockState *state = owner.getLevelFor(actor).peekBlockPtr(x, y, z);
        return state != nullptr && isFireBlock(state->mName);
    }

    bool sameBlock(const Vector3f &first, const Vector3f &second) {
        return std::floor(first.x) == std::floor(second.x) && std::floor(first.y) == std::floor(second.y)
               && std::floor(first.z) == std::floor(second.z);
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
        if (!player.sharesLevelWith(actor))
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


        EntityDataEntry flags3;

        flags3.mId = ActorFlags::FLAGS_3_DATA_ID;

        flags3.mFormat = EntityDataFormat::Long;

        flags3.mLongValue = actor.getFlags().getExtendedBits();
add.mMetadata.mEntries.push_back(flags3);


        EntityDataEntry nameplateDistance;

        nameplateDistance.mId = ActorFlags::NAMEPLATE_RENDER_DISTANCE_DATA_ID;

        nameplateDistance.mFormat = EntityDataFormat::Float;

        nameplateDistance.mFloatValue = actor.getNameplateRenderDistance();

        add.mMetadata.mEntries.push_back(nameplateDistance);

        owner.getNetworkHandler().send(player.getNetworkIdentifier(), add, owner.getCodecContext());
    }

    void broadcastItemActorSpawn(ServerNetworkHandler &owner, const ItemActor &actor) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned())
                spawnItemActorTo(owner, entry.second, actor);
        }
    }

    void broadcastItemActorMove(ServerNetworkHandler &owner, const ItemActor &actor, bool teleported = false) {
        MoveActorAbsolutePacket move;
        move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
        move.mPosition = actor.getPosition();
        move.mRotation = actor.getRotation();
        move.mOnGround = actor.isOnGround();
        move.mTeleported = teleported;
        move.mForceMove = false;

        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned() && entry.second.sharesLevelWith(actor))
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), move, owner.getCodecContext());
        }
    }

    void broadcastItemActorRemove(ServerNetworkHandler &owner, const ItemActor &actor) {
        RemoveActorPacket remove;
        remove.mUniqueActorId = actor.getUniqueId();

        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned() && entry.second.sharesLevelWith(actor))
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), remove, owner.getCodecContext());
        }
    }

    bool hasSupportBelow(ServerNetworkHandler &owner, const ItemActor &actor) {
        const Vector3f &position = actor.getPosition();

        return owner.getLevelFor(actor).isSolidAt((int32_t) std::floor(position.x),
                                                  (int32_t) std::floor(position.y - ITEM_GROUND_PROBE_DEPTH),
                                                  (int32_t) std::floor(position.z));
    }

    // top an item rests on in a solid cell: a snow layer's look, the cell top for other blocks
    float itemFloor(Level &level, const ItemActor &actor, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
        if (block == nullptr || !block->shapesCollisionPerActor())
            return (float) (y + 1);

        return (float) y + BlockShape::getRelativeShape(*state, &actor).mMaxY;
    }

    bool blocksItem(Level &level, const ItemActor &actor, int32_t x, int32_t y, int32_t z, float feetY) {
        return level.isSolidAt(x, y, z) && feetY < itemFloor(level, actor, x, y, z);
    }

    // a snow layer grown under a resting item lifts it to its new look
    void liftOutOfLayer(ServerNetworkHandler &owner, ItemActor &actor) {
        Level &level = owner.getLevelFor(actor);
        Vector3f position = actor.getPosition();
        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t y = (int32_t) std::floor(position.y);
        const int32_t z = (int32_t) std::floor(position.z);
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
        if (block == nullptr || !block->shapesCollisionPerActor())
            return;

        const float top = itemFloor(level, actor, x, y, z);
        if (position.y >= top)
            return;

        position.y = top;
        actor.setPosition(position);
        broadcastItemActorMove(owner, actor);
    }

    void moveItemActor(ServerNetworkHandler &owner, ItemActor &actor) {
        if (actor.isOnGround()) {
            if (hasSupportBelow(owner, actor)) {
                liftOutOfLayer(owner, actor);
                return;
            }

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
            if (blocksItem(level, actor, (int32_t) std::floor(candidateX), currentY, currentZ, position.y))
                motion.x = 0.0f;
            else
                next.x = candidateX;
        }

        if (motion.z != 0.0f) {
            const float candidateZ = position.z + motion.z;
            if (blocksItem(level, actor, (int32_t) std::floor(next.x), currentY, (int32_t) std::floor(candidateZ),
                           position.y))
                motion.z = 0.0f;
            else
                next.z = candidateZ;
        }

        next.y = position.y + motion.y;

        const int32_t blockX = (int32_t) std::floor(next.x);
        const int32_t blockZ = (int32_t) std::floor(next.z);
        const int32_t blockY = (int32_t) std::floor(next.y);

        bool solid = level.isSolidAt(blockX, blockY, blockZ);
        float landing = solid ? itemFloor(level, actor, blockX, blockY, blockZ) : 0.0f;
        // a fall can cross a low floor such as a snow layer's look in one tick: land on the first one crossed
        if (motion.y < 0.0f) {
            for (int32_t y = (int32_t) std::floor(position.y); y > blockY; --y) {
                if (!level.isSolidAt(blockX, y, blockZ))
                    continue;
                const float floor = itemFloor(level, actor, blockX, y, blockZ);
                if (position.y >= floor && next.y < floor) {
                    solid = true;
                    landing = floor;
                    break;
                }
            }
        }
        if (motion.y < 0.0f && solid && next.y < landing) {
            next.y = landing;
            motion = Vector3f(0.0f, 0.0f, 0.0f);
            actor.setOnGround(true);
        } else if (motion.y > 0.0f && solid) {
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

            if (player.isSpectator())
                continue;

            if (!player.sharesLevelWith(actor))
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

            ItemStack eventItem = pickedItem;
            PluginEvent pluginEvent;
            pluginEvent.mType = FALCON_EVENT_PLAYER_PICKUP_ITEM;
            pluginEvent.mCancellable = true;
            pluginEvent.mPlayer = &player;
            pluginEvent.mItem = &eventItem;
            PluginManager::getInstance().dispatch(pluginEvent);
            if (actor.isRemoved())
                return;
            if (pluginEvent.mCancelled)
                continue;
            if (owner.getScriptEngine().beforeEntityItemPickup(player, actor, pickedItem)) {
                if (actor.isRemoved())
                    return;
                continue;
            }

            std::vector<int> touchedSlots;
            if (!inventory.addItem(actor.getItem(), touchedSlots))
                continue;

            if (pickedItem.mDefinition != nullptr) {
                EntityItemPickupAfterEvent pickupEvent(player, pickedItem.mDefinition->getIdentifier(),
                                                       pickedItem.mCount);
                owner.getEventBus().after().mEntityItemPickup.emit(pickupEvent);
                owner.getScriptEngine().onEntityItemPickup(player, pickedItem);
            }

            for (int slot: touchedSlots)
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

            ItemActorHandler::collect(owner, actor, player);
            return;
        }
    }
}

ItemActor *ItemActorHandler::dropItem(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                      const ItemStack &item, const Vector3f &motion, int pickupDelay) {
    if (item.isAir() || item.mCount <= 0)
        return nullptr;

    std::unique_ptr<ItemActor> actor(new ItemActor(owner.allocateRuntimeId(), item));
    actor->setUniqueId(owner.allocateActorUniqueId());
    actor->moveToLevel(level);
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

void ItemActorHandler::collect(ServerNetworkHandler &owner, ItemActor &item, const Actor &collector) {
    TakeItemActorPacket take;
    take.mItemRuntimeActorId = item.getRuntimeId();
    take.mRuntimeActorId = collector.getRuntimeId();

    for (auto &viewer: owner.getPlayers()) {
        if (viewer.second.isSpawned() && viewer.second.sharesLevelWith(item))
            owner.getNetworkHandler().send(viewer.second.getNetworkIdentifier(), take, owner.getCodecContext());
    }

    item.setRemoved(true);
}

void ItemActorHandler::refresh(ServerNetworkHandler &owner, const ItemActor &item) {
    broadcastItemActorRemove(owner, item);
    broadcastItemActorSpawn(owner, item);
}

void ItemActorHandler::teleport(ServerNetworkHandler &owner, Level &level, ItemActor &item, const Vector3f &position) {
    item.teleport(position);

    const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
    const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
    const int64_t column = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    if (owner.getWorldOf(level).getActorLoadedChunks(level.getDimensionType()).count(column) != 0) {
        broadcastItemActorMove(owner, item, true);
        return;
    }

    // kept in the saved entities of its column until the column loads, as other actors are
    if (level.isStorageOpen()) {
        std::vector<Tag> entities = level.loadEntities(chunkX, chunkZ);
        entities.push_back(item.saveNbt());
        level.saveEntities(chunkX, chunkZ, entities);
    }

    broadcastItemActorRemove(owner, item);
    std::vector<std::unique_ptr<ItemActor>> &actors = owner.getItemEntities();
    actors.erase(std::remove_if(actors.begin(), actors.end(),
                                [&item](const std::unique_ptr<ItemActor> &entry) { return entry.get() == &item; }),
                 actors.end());
    level.releaseChunkIfUnused(chunkX, chunkZ);
}

void ItemActorHandler::sendItemActorsTo(ServerNetworkHandler &owner, ServerPlayer &player) {
    for (const std::unique_ptr<ItemActor> &actor: owner.getItemEntities()) {
        if (!actor->isRemoved())
            spawnItemActorTo(owner, player, *actor);
    }
}

void ItemActorHandler::tickItemActors(ServerNetworkHandler &owner) {
    std::vector<std::unique_ptr<ItemActor>> &actors = owner.getItemEntities();

    // by index: a pickup handler can drop an item (the vector grows) or move one out of it
    for (size_t index = 0; index < actors.size();) {
        ItemActor &actor = *actors[index];

        actor.tick();
        if (isInFire(owner, actor) && !isFireResistant(actor.getItem()))
            actor.setRemoved(true);

        if (!actor.isRemoved()) {
            const Vector3f previous = actor.getPosition();
            moveItemActor(owner, actor);

            // same cell: the block under it was checked just above
            if (!sameBlock(previous, actor.getPosition()) && isInFire(owner, actor)
                && !isFireResistant(actor.getItem()))
                actor.setRemoved(true);
        }

        tryPickupItemActor(owner, actor);

        // erased by a handler: the next one took its place
        if (index >= actors.size() || actors[index].get() != &actor)
            continue;

        if (actor.isRemoved() || actor.isExpired()) {
            broadcastItemActorRemove(owner, actor);
            owner.getLevelFor(actor).eraseEntity(actor.getUniqueId());
            actors.erase(actors.begin() + (long) index);
            continue;
        }

        ++index;
    }
}

ItemActor *ItemActorHandler::restoreItem(ServerNetworkHandler &owner, Level &level, const Tag &data) {
    std::unique_ptr<ItemActor> actor(new ItemActor(owner.allocateRuntimeId(), ItemStack::air()));
    actor->loadNbt(data, owner.getCodecContext());
    if (actor->getItem().isAir() || actor->getItem().mCount <= 0)
        return nullptr;

    for (const std::unique_ptr<ItemActor> &existing: owner.getItemEntities()) {
        if (existing->getUniqueId() == actor->getUniqueId())
            return nullptr;
    }

    if (!actor->hasAssignedUniqueId())
        actor->setUniqueId(owner.allocateActorUniqueId());

    actor->moveToLevel(level);
    actor->getItem().mUsingNetId = false;
    actor->getItem().mNetId = 0;

    ItemActor *result = actor.get();
    owner.getItemEntities().push_back(std::move(actor));

    broadcastItemActorSpawn(owner, *result);
    return result;
}

void ItemActorHandler::saveItemsInChunk(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ,
                                        std::vector<Tag> &out, bool cull) {
    std::vector<std::unique_ptr<ItemActor>> &actors = owner.getItemEntities();

    for (auto it = actors.begin(); it != actors.end();) {
        ItemActor &actor = **it;
        const Vector3f &position = actor.getPosition();
        if (actor.isRemoved() || !actor.isIn(level) || ((int32_t) std::floor(position.x) >> 4) != chunkX
            || ((int32_t) std::floor(position.z) >> 4) != chunkZ) {
            ++it;
            continue;
        }

        out.push_back(actor.saveNbt());
        if (!cull) {
            ++it;
            continue;
        }

        broadcastItemActorRemove(owner, actor);
        it = actors.erase(it);
    }
}

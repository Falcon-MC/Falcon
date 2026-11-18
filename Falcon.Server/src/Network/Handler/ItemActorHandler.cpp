#include "Network/Handler/ItemActorHandler.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockNames.h"
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
#include "Scripting/Content/CustomContentRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
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

// 0.15 to 0.85 inside the block on each axis
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
    const int RESTING_MOVE_MASK = 3;
    // every 25 ticks an item looks for its like half a block around its box
    const int MERGE_INTERVAL = 25;
    const float MERGE_REACH = 0.5f;
    const float ITEM_HALF_WIDTH = 0.125f;
    const float ITEM_HEIGHT = 0.25f;
    const uint8_t ACTOR_EVENT_UPDATE_STACK_SIZE = 69;
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

        if (const CustomItemDefinition *custom =
                    CustomContentRegistry::getInstance().findItem(item.mDefinition->getIdentifier()))
            return custom->mFireResistant;

        const FireResistantItemComponent *component =
                ItemDataTable::getComponents(item.mDefinition->getIdentifier()).get<FireResistantItemComponent>();
        return component != nullptr && component->isFireResistant();
    }

    // a pack item can opt out of the five minute lifetime
    bool despawns(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return true;

        const CustomItemDefinition *custom =
                CustomContentRegistry::getInstance().findItem(item.mDefinition->getIdentifier());
        return custom == nullptr || custom->mShouldDespawn;
    }

    bool isInFire(ServerNetworkHandler &owner, const ItemActor &actor) {
        const Vector3f &position = actor.getPosition();
        const int x = (int) std::floor(position.x);
        const int y = (int) std::floor(position.y);
        const int z = (int) std::floor(position.z);
        const BlockState *state = owner.getLevelFor(actor).peekBlockPtr(x, y, z);
        // most items lie in air: nothing to look up
        return state != nullptr && state->mName != BlockNames::AIR && isFireBlock(state->mName);
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
        Level &level = owner.getLevelFor(actor);
        const Vector3f position = actor.getPosition();
        // an item waits at the edge of the loaded chunks: neither its rest nor its fall makes the terrain beyond
        const int32_t fromChunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t fromChunkZ = (int32_t) std::floor(position.z) >> 4;
        if (!level.isChunkResident(fromChunkX, fromChunkZ))
            return;

        if (actor.isOnGround()) {
            // the game moves an item at rest on the ground on one tick in four
            if ((actor.getAge() & RESTING_MOVE_MASK) != 0)
                return;

            if (hasSupportBelow(owner, actor)) {
                liftOutOfLayer(owner, actor);
                return;
            }

            actor.setOnGround(false);
        }

        Vector3f motion = actor.getMotion();
        const int32_t toChunkX = (int32_t) std::floor(position.x + motion.x) >> 4;
        const int32_t toChunkZ = (int32_t) std::floor(position.z + motion.z) >> 4;
        if ((toChunkX != fromChunkX || toChunkZ != fromChunkZ)
            && (!level.isChunkResident(toChunkX, fromChunkZ) || !level.isChunkResident(fromChunkX, toChunkZ)
                || !level.isChunkResident(toChunkX, toChunkZ)))
            return;

        motion.y -= ITEM_GRAVITY;

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
            const int pickupCount = std::min(freeSpaceFor(inventory, actor.getItem()), actor.getItem().mCount);
            if (pickupCount <= 0)
                continue;

            ItemStack pickedItem = actor.getItem();
            pickedItem.mCount = pickupCount;

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
            const int remaining = inventory.addItemPartial(pickedItem, touchedSlots);
            const int collectedCount = pickupCount - remaining;
            if (collectedCount <= 0)
                continue;
            pickedItem.mCount = collectedCount;
            actor.getItem().mCount -= collectedCount;

            if (pickedItem.mDefinition != nullptr) {
                EntityItemPickupAfterEvent pickupEvent(player, pickedItem.mDefinition->getIdentifier(),
                                                       collectedCount);
                owner.getEventBus().after().mEntityItemPickup.emit(pickupEvent);
                owner.getScriptEngine().onEntityItemPickup(player, pickedItem);
            }

            for (int slot: touchedSlots)
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

            if (actor.getItem().mCount <= 0)
                ItemActorHandler::collect(owner, actor, player);
            else
                ItemActorHandler::refresh(owner, actor);
            return;
        }
    }
}

namespace {
    int64_t mergeCell(const Level *level, int32_t x, int32_t y, int32_t z) {
        return ((int64_t) (x & 0x3FFFFF) << 42) ^ ((int64_t) (z & 0x3FFFFF) << 20) ^ ((int64_t) (y & 0xFFF) << 8)
               ^ (int64_t) (reinterpret_cast<uintptr_t>(level) >> 4 & 0xFF);
    }

    // the smaller stack joins the larger one when both fit in one stack; the stack kept takes the longer
    // pickup delay and the younger age
    void mergeItemActors(ServerNetworkHandler &owner) {
        std::vector<std::unique_ptr<ItemActor>> &actors = owner.getItemEntities();
        std::vector<ItemActor *> due;
        for (const std::unique_ptr<ItemActor> &actor: actors) {
            if (!actor->isRemoved() && actor->getAge() > 0 && actor->getAge() % MERGE_INTERVAL == 0)
                due.push_back(actor.get());
        }
        if (due.empty())
            return;

        // the items by cell, chained in the order of the list through tables kept from one tick to the
        // next: nothing is allocated per cell
        struct Slot {
            int64_t mCell;
            int32_t mFirst;
            int32_t mLast;
        };
        thread_local std::vector<Slot> slots;
        thread_local std::vector<int32_t> nextInCell;
        size_t capacity = 64;
        while (capacity < actors.size() * 2)
            capacity <<= 1;
        slots.assign(capacity, Slot{0, -1, -1});
        nextInCell.assign(actors.size(), -1);
        const auto slotOf = [&](int64_t cell) -> Slot & {
            size_t at = (size_t) ((uint64_t) cell * 0x9E3779B97F4A7C15ull >> 32) & (capacity - 1);
            while (slots[at].mFirst >= 0 && slots[at].mCell != cell)
                at = (at + 1) & (capacity - 1);
            return slots[at];
        };
        for (size_t index = 0; index < actors.size(); ++index) {
            const ItemActor &actor = *actors[index];
            if (actor.isRemoved())
                continue;
            const Vector3f &position = actor.getPosition();
            const int64_t cell = mergeCell(&owner.getLevelFor(actor), (int32_t) std::floor(position.x),
                                           (int32_t) std::floor(position.y), (int32_t) std::floor(position.z));
            Slot &slot = slotOf(cell);
            if (slot.mFirst < 0) {
                slot.mCell = cell;
                slot.mFirst = (int32_t) index;
            } else {
                nextInCell[(size_t) slot.mLast] = (int32_t) index;
            }
            slot.mLast = (int32_t) index;
        }

        const float reach = MERGE_REACH + ITEM_HALF_WIDTH * 2.0f;
        for (ItemActor *self: due) {
            if (self->isRemoved())
                continue;

            const Level *level = &owner.getLevelFor(*self);
            const Vector3f origin = self->getPosition();
            const int32_t minX = (int32_t) std::floor(origin.x - reach), maxX = (int32_t) std::floor(origin.x + reach);
            const int32_t minY = (int32_t) std::floor(origin.y - MERGE_REACH - ITEM_HEIGHT);
            const int32_t maxY = (int32_t) std::floor(origin.y + MERGE_REACH + ITEM_HEIGHT);
            const int32_t minZ = (int32_t) std::floor(origin.z - reach), maxZ = (int32_t) std::floor(origin.z + reach);
            bool merged = false;
            for (int32_t x = minX; x <= maxX && !merged; ++x) {
                for (int32_t y = minY; y <= maxY && !merged; ++y) {
                    for (int32_t z = minZ; z <= maxZ && !merged; ++z) {
                        const Slot &cell = slotOf(mergeCell(level, x, y, z));
                        for (int32_t link = cell.mFirst; link >= 0; link = nextInCell[(size_t) link]) {
                            ItemActor *other = actors[(size_t) link].get();
                            if (other == self || other->isRemoved() || &owner.getLevelFor(*other) != level)
                                continue;

                            const Vector3f &at = other->getPosition();
                            if (std::fabs(at.x - origin.x) > reach || std::fabs(at.z - origin.z) > reach
                                || at.y > origin.y + ITEM_HEIGHT + MERGE_REACH
                                || at.y + ITEM_HEIGHT < origin.y - MERGE_REACH)
                                continue;

                            const ItemStack &mine = self->getItem();
                            const ItemStack &theirs = other->getItem();
                            if (mine.isAir() || theirs.isAir() || !PlayerInventory::canStack(mine, theirs)
                                || mine.mCount + theirs.mCount > PlayerInventory::getMaxStackSize(mine))
                                continue;

                            ItemActor *kept = theirs.mCount >= mine.mCount ? other : self;
                            ItemActor *gone = kept == other ? self : other;
                            ItemStack stack = kept->getItem();
                            stack.mCount += gone->getItem().mCount;
                            kept->setItem(stack);
                            kept->setPickupDelay(std::max(kept->getPickupDelay(), gone->getPickupDelay()));
                            kept->setAge(std::min(kept->getAge(), gone->getAge()));
                            gone->setRemoved(true);
                            owner._broadcastEntityEvent(*kept, ACTOR_EVENT_UPDATE_STACK_SIZE, stack.mCount);
                            // one merge a look, as the game: the stack kept looks again 25 ticks on
                            merged = true;
                            break;
                        }
                    }
                }
            }
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

    std::vector<ItemActor *> gone;
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

        if (actor.isRemoved() || (actor.isExpired() && despawns(actor.getItem()))) {
            broadcastItemActorRemove(owner, actor);
            owner.getLevelFor(actor).eraseEntity(actor.getUniqueId());
            // taken out of the list in one sweep below: an explosion removes thousands a tick, and each
            // one taken out alone moves all the others
            actor.setRemoved(true);
            gone.push_back(&actor);
        }

        ++index;
    }

    if (!gone.empty()) {
        // in the order of the list, which nothing shuffles
        size_t next = 0;
        size_t kept = 0;
        for (size_t index = 0; index < actors.size(); ++index) {
            if (next < gone.size() && actors[index].get() == gone[next]) {
                ++next;
                continue;
            }
            if (kept != index)
                actors[kept] = std::move(actors[index]);
            ++kept;
        }
        actors.resize(kept);
    }

    mergeItemActors(owner);
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

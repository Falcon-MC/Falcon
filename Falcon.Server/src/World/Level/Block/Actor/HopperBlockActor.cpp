#include "World/Level/Block/Actor/HopperBlockActor.h"

#include "World/Level/Block/ComposterBlock.h"
#include "World/Actor/Misc/ItemActor.h"
#include "Core/Math/AxisAlignedBB.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"

#include <algorithm>
#include <utility>

namespace {
    const char *HOPPER = "minecraft:hopper";
    const char *TAG_TRANSFER_COOLDOWN = "TransferCooldown";
    const char *STATE_FACING_DIRECTION = "facing_direction";
    const char *STATE_TOGGLE_BIT = "toggle_bit";

    const int FACE_DOWN = 0;

    const int32_t FACE_OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    struct SlotRange {
        int mFirst = 0;
        int mLast = 0;
    };

    int facingOf(const BlockState &state) {
        const Tag *facing = state.mStates.get(STATE_FACING_DIRECTION);
        if (facing == nullptr)
            return FACE_DOWN;

        if (facing->getType() == Tag::Type::Byte)
            return (int) facing->asByte();

        if (facing->getType() == Tag::Type::Int)
            return (int) facing->asInt();

        return FACE_DOWN;
    }

    bool isDisabled(const BlockState &state) {
        const Tag *toggle = state.mStates.get(STATE_TOGGLE_BIT);
        if (toggle == nullptr || toggle->getType() != Tag::Type::Byte)
            return false;

        return toggle->asByte() != 0;
    }

    SlotRange fullRange(const Container &container) {
        SlotRange range;
        range.mFirst = 0;
        range.mLast = container.getContainerSize();
        return range;
    }

    SlotRange pushRange(BlockActor &target, Container &container, bool fromAbove) {
        SlotRange range;

        if (dynamic_cast<FurnaceBlockActor *>(&target) != nullptr) {
            range.mFirst = fromAbove ? FurnaceInventory::SLOT_INPUT : FurnaceInventory::SLOT_FUEL;
            range.mLast = range.mFirst + 1;
            return range;
        }

        if (dynamic_cast<BrewingStandBlockActor *>(&target) != nullptr) {
            range.mFirst = fromAbove ? BrewingStandBlockActor::SLOT_INGREDIENT : 1;
            range.mLast = fromAbove ? 1 : BrewingStandBlockActor::SLOT_FUEL;
            return range;
        }

        return fullRange(container);
    }

    SlotRange pullRange(BlockActor &source, Container &container) {
        SlotRange range;

        if (dynamic_cast<FurnaceBlockActor *>(&source) != nullptr) {
            range.mFirst = FurnaceInventory::SLOT_OUTPUT;
            range.mLast = range.mFirst + 1;
            return range;
        }

        if (dynamic_cast<BrewingStandBlockActor *>(&source) != nullptr) {
            range.mFirst = 1;
            range.mLast = BrewingStandBlockActor::SLOT_FUEL;
            return range;
        }

        return fullRange(container);
    }

    bool addOneWithin(Container &container, const SlotRange &range, const ItemStack &item) {
        if (!container.canHold(item))
            return false;

        const int maxStackSize = PlayerInventory::getMaxStackSize(item);

        for (int slot = range.mFirst; slot < range.mLast; ++slot) {
            const ItemStack &existing = container.getContainerItem(slot);
            if (existing.isAir() || existing.mCount <= 0 || container.isSlotDisabled(slot))
                continue;

            if (!container.canHoldInSlot(slot, item))
                continue;

            if (!PlayerInventory::canStack(existing, item) || existing.mCount >= maxStackSize)
                continue;

            ItemStack updated = existing;
            ++updated.mCount;
            container.setContainerItem(slot, std::move(updated));
            return true;
        }

        for (int slot = range.mFirst; slot < range.mLast; ++slot) {
            const ItemStack &existing = container.getContainerItem(slot);
            if ((!existing.isAir() && existing.mCount > 0) || container.isSlotDisabled(slot))
                continue;

            if (!container.canHoldInSlot(slot, item))
                continue;

            ItemStack single = item;
            single.mCount = 1;
            container.setContainerItem(slot, std::move(single));
            return true;
        }

        return false;
    }

    int addItemsWithin(Container &container, const SlotRange &range, const ItemStack &item) {
        if (!container.canHold(item))
            return 0;

        int remaining = item.mCount;
        const int maxStackSize = PlayerInventory::getMaxStackSize(item);

        for (int slot = range.mFirst; slot < range.mLast && remaining > 0; ++slot) {
            const ItemStack &existing = container.getContainerItem(slot);
            if (existing.isAir() || existing.mCount <= 0 || !PlayerInventory::canStack(existing, item))
                continue;

            const int moved = std::min(remaining, maxStackSize - existing.mCount);
            if (moved <= 0)
                continue;

            ItemStack updated = existing;
            updated.mCount += moved;
            container.setContainerItem(slot, std::move(updated));
            remaining -= moved;
        }

        for (int slot = range.mFirst; slot < range.mLast && remaining > 0; ++slot) {
            const ItemStack &existing = container.getContainerItem(slot);
            if (!existing.isAir() && existing.mCount > 0)
                continue;

            const int moved = std::min(remaining, maxStackSize);
            ItemStack inserted = item;
            inserted.mCount = moved;
            container.setContainerItem(slot, std::move(inserted));
            remaining -= moved;
        }

        return item.mCount - remaining;
    }

    // FALCON_EVENT_HOPPER_TRANSFER for the one item about to move; false when a plugin keeps it
    bool allowedByPlugins(Level *level, const Vector3i &hopper, const ItemStack &source, bool pushing) {
        PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_HOPPER_TRANSFER);
        if (plugins == nullptr)
            return true;

        ItemStack one = source;
        one.mCount = 1;
        PluginEvent event;
        event.mType = FALCON_EVENT_HOPPER_TRANSFER;
        event.mCancellable = true;
        event.mLevel = level;
        event.mBlockPosition = hopper;
        event.mItem = &one;
        event.mState = pushing;
        plugins->dispatch(event);
        return !event.mCancelled;
    }

    // a container actor (a chest or hopper minecart, a chest boat) whose box reaches into the block
    Container *actorContainerAt(Level &level, const Vector3i &block) {
        ServerNetworkHandler *owner = level.getOwner();
        if (owner == nullptr)
            return nullptr;
        const AxisAlignedBB cell((float) block.x, (float) block.y, (float) block.z, (float) block.x + 1.0f,
                                 (float) block.y + 1.0f, (float) block.z + 1.0f);
        Container *found = nullptr;
        owner->forEachActorNear(Vector3f((float) block.x + 0.5f, (float) block.y, (float) block.z + 0.5f), 1.0f,
                                [&](ServerActor &actor) {
            if (found != nullptr || !actor.isAlive() || actor.isExpired() || !actor.isIn(level))
                return;
            Container *container = actor.getEntityContainer();
            if (container != nullptr && actor.isMinecart()
                && ActorPushSystem::boundingBoxOf(actor).intersectsWith(cell))
                found = container;
        });
        return found;
    }

    bool isFull(const Container &container) {
        for (int slot = 0; slot < container.getContainerSize(); ++slot) {
            const ItemStack &item = container.getContainerItem(slot);
            if (item.isAir() || item.mCount <= 0 || item.mCount < PlayerInventory::getMaxStackSize(item))
                return false;
        }
        return true;
    }

    bool moveOne(Container &from, const SlotRange &fromRange, Container &to, const SlotRange &toRange, Level *level,
                 const Vector3i &hopper, bool pushing) {
        for (int slot = fromRange.mFirst; slot < fromRange.mLast; ++slot) {
            const ItemStack &source = from.getContainerItem(slot);
            if (source.isAir() || source.mCount <= 0 || !from.canTakeFromSlot(slot))
                continue;
            if (!allowedByPlugins(level, hopper, source, pushing))
                return false;

            ItemStack moved = source;
            if (!addOneWithin(to, toRange, moved))
                continue;

            ItemStack remaining = source;
            --remaining.mCount;
            from.setContainerItem(slot, remaining.mCount <= 0 ? ItemStack::air() : std::move(remaining));
            return true;
        }

        return false;
    }
}

bool HopperBlockActor::addOne(Container &container, const ItemStack &item) {
    return addOneWithin(container, fullRange(container), item);
}

Tag HopperBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putInt(TAG_TRANSFER_COOLDOWN, mTransferCooldown);
    return data;
}

void HopperBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mTransferCooldown = data.getInt(TAG_TRANSFER_COOLDOWN, COOLDOWN_TICKS);
}

bool HopperBlockActor::tick(ServerNetworkHandler &owner) {
    if (mTransferCooldown > 0) {
        --mTransferCooldown;
        return true;
    }

    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *current = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (current == nullptr)
        return true;

    if (current->mName != HOPPER)
        return false;

    const BlockState state = *current;

    if (isDisabled(state))
        return true;

    bool changed = _pushItems(level, state);
    if (_pullItems(owner))
        changed = true;

    if (changed) {
        mTransferCooldown = COOLDOWN_TICKS;

        owner.refreshContainerViewers(mPosition);
        owner.refreshContainerViewers(Vector3i(mPosition.x, mPosition.y + 1, mPosition.z));

        const int facing = facingOf(state);
        if (facing >= 0 && facing < 6)
            owner.refreshContainerViewers(Vector3i(mPosition.x + FACE_OFFSETS[facing][0],
                                                   mPosition.y + FACE_OFFSETS[facing][1],
                                                   mPosition.z + FACE_OFFSETS[facing][2]));
    }
    return true;
}

bool HopperBlockActor::_pushItems(Level &level, const BlockState &state) {
    if (mInventory.isEmpty())
        return false;

    const int facing = facingOf(state);
    if (facing < 0 || facing >= 6)
        return false;

    const Vector3i target(mPosition.x + FACE_OFFSETS[facing][0], mPosition.y + FACE_OFFSETS[facing][1],
                          mPosition.z + FACE_OFFSETS[facing][2]);

    BlockActor *targetActor = level.getBlockActors().find(target);
    Container *targetContainer = targetActor == nullptr ? nullptr : targetActor->getContainer();
    if (targetContainer != nullptr)
        return moveOne(mInventory, fullRange(mInventory), *targetContainer,
                       pushRange(*targetActor, *targetContainer, facing == FACE_DOWN), &level, mPosition, true);

    // a composter below takes the first slot it composts, one item at a time
    const BlockState *below = facing == FACE_DOWN ? level.peekBlockPtr(target.x, target.y, target.z) : nullptr;
    if (below != nullptr && ComposterBlock::matches(below->mName) && level.getOwner() != nullptr) {
        for (int slot = 0; slot < mInventory.getContainerSize(); ++slot) {
            ItemStack item = mInventory.getContainerItem(slot);
            if (item.isAir() || ComposterBlock::compostChance(item) <= 0)
                continue;
            if (!allowedByPlugins(&level, mPosition, item, true))
                return false;
            if (!ComposterBlock::addItem(*level.getOwner(), level, target, item))
                continue;
            --item.mCount;
            mInventory.setContainerItem(slot, item.mCount <= 0 ? ItemStack::air() : std::move(item));
            return true;
        }
        return false;
    }

    // a minecart with a container in front takes the items when no block container is there
    Container *minecart = actorContainerAt(level, target);
    return minecart != nullptr
           && moveOne(mInventory, fullRange(mInventory), *minecart, fullRange(*minecart), &level, mPosition, true);
}

bool HopperBlockActor::_pullItems(ServerNetworkHandler &owner) {
    if (mInventory.isFull() || mLevel == nullptr)
        return false;

    // a container above is pulled from; anything else lets the hopper pick up dropped items
    const Vector3i above(mPosition.x, mPosition.y + 1, mPosition.z);
    BlockActor *source = mLevel->getBlockActors().find(above);
    if (source != nullptr && source->getContainer() != nullptr)
        return pullFrom(owner, *mLevel, above, mInventory, mPosition);
    if (Container *minecart = actorContainerAt(*mLevel, above))
        return moveOne(*minecart, fullRange(*minecart), mInventory, fullRange(mInventory), mLevel, mPosition, false);
    return _pickupItemActors(owner);
}

bool HopperBlockActor::_pickupItemActors(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return false;
    const float minX = (float) mPosition.x;
    const float minY = (float) mPosition.y;
    const float minZ = (float) mPosition.z;
    return pickUpItems(owner, *mLevel, minX, minY, minZ, minX + 1.0f, minY + 2.0f, minZ + 1.0f, mInventory);
}

bool HopperBlockActor::pullFrom(ServerNetworkHandler &owner, Level &level, const Vector3i &source, Container &target,
                                const Vector3i &puller) {
    if (isFull(target))
        return false;

    BlockActor *sourceActor = level.getBlockActors().find(source);
    if (sourceActor == nullptr)
        return false;

    sourceActor->unpackLoot(owner);
    Container *sourceContainer = sourceActor->getContainer();
    if (sourceContainer == nullptr)
        return false;

    return moveOne(*sourceContainer, pullRange(*sourceActor, *sourceContainer), target, fullRange(target), &level,
                   puller, false);
}

bool HopperBlockActor::pickUpItems(ServerNetworkHandler &owner, Level &level, float minX, float minY, float minZ,
                                   float maxX, float maxY, float maxZ, Container &target) {
    bool pickedUp = false;

    for (const std::unique_ptr<ItemActor> &entry: owner.getItemEntities()) {
        if (isFull(target))
            break;

        ItemActor &actor = *entry;
        if (actor.isRemoved() || !actor.isIn(level))
            continue;

        const Vector3f &position = actor.getPosition();
        if (position.x < minX || position.x >= maxX || position.y < minY || position.y >= maxY || position.z < minZ
            || position.z >= maxZ)
            continue;

        ItemStack remaining = actor.getItem();
        if (remaining.isAir() || remaining.mCount <= 0)
            continue;

        const int moved = addItemsWithin(target, fullRange(target), remaining);
        if (moved <= 0)
            continue;

        remaining.mCount -= moved;
        if (remaining.mCount <= 0)
            actor.setRemoved(true);
        else
            actor.setItem(remaining);

        pickedUp = true;
    }

    return pickedUp;
}

#include "Block/Actor/HopperBlockActor.h"

#include "Actor/Misc/ItemActor.h"
#include "Block/Actor/FurnaceBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <utility>

namespace {
    const char *HOPPER = "minecraft:hopper";
    const char *TAG_TRANSFER_COOLDOWN = "TransferCooldown";
    const char *STATE_FACING_DIRECTION = "facing_direction";
    const char *STATE_TOGGLE_BIT = "toggle_bit";

    const int FACE_DOWN = 0;

    const int32_t FACE_OFFSETS[6][3] = {
            {0,  -1, 0},
            {0,  1,  0},
            {0,  0,  -1},
            {0,  0,  1},
            {-1, 0,  0},
            {1,  0,  0}
    };

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
        const int maxStackSize = PlayerInventory::getMaxStackSize(item);

        for (int slot = range.mFirst; slot < range.mLast; ++slot) {
            const ItemStack &existing = container.getContainerItem(slot);
            if (existing.isAir() || existing.mCount <= 0)
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
            if (!existing.isAir() && existing.mCount > 0)
                continue;

            ItemStack single = item;
            single.mCount = 1;
            container.setContainerItem(slot, std::move(single));
            return true;
        }

        return false;
    }

    int addItemsWithin(Container &container, const SlotRange &range, const ItemStack &item) {
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

    bool moveOne(Container &from, const SlotRange &fromRange, Container &to, const SlotRange &toRange) {
        for (int slot = fromRange.mFirst; slot < fromRange.mLast; ++slot) {
            const ItemStack &source = from.getContainerItem(slot);
            if (source.isAir() || source.mCount <= 0)
                continue;

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
    if (targetActor == nullptr)
        return false;

    HopperBlockActor *targetHopper = dynamic_cast<HopperBlockActor *>(targetActor);
    if (targetHopper != nullptr && facing != FACE_DOWN)
        return false;

    Container *targetContainer = targetActor->getContainer();
    if (targetContainer == nullptr)
        return false;

    return moveOne(mInventory, fullRange(mInventory), *targetContainer,
                   pushRange(*targetActor, *targetContainer, facing == FACE_DOWN));
}

bool HopperBlockActor::_pullItems(ServerNetworkHandler &owner) {
    if (mInventory.isFull())
        return false;

    const Vector3i above(mPosition.x, mPosition.y + 1, mPosition.z);

    BlockActor *sourceActor = mLevel->getBlockActors().find(above);
    if (sourceActor == nullptr)
        return _pickupItemActors(owner);

    Container *sourceContainer = sourceActor->getContainer();
    if (sourceContainer == nullptr)
        return _pickupItemActors(owner);

    return moveOne(*sourceContainer, pullRange(*sourceActor, *sourceContainer), mInventory,
                   fullRange(mInventory));
}

bool HopperBlockActor::_pickupItemActors(ServerNetworkHandler &owner) {
    const float minX = (float) mPosition.x;
    const float minY = (float) mPosition.y;
    const float minZ = (float) mPosition.z;
    const float maxX = minX + 1.0f;
    const float maxY = minY + 2.0f;
    const float maxZ = minZ + 1.0f;

    bool pickedUp = false;

    for (const std::unique_ptr<ItemActor> &entry: owner.getItemEntities()) {
        if (mInventory.isFull())
            break;

        ItemActor &actor = *entry;
        if (actor.isRemoved() || mLevel == nullptr || actor.getDimension() != mLevel->getDimensionType())
            continue;

        const Vector3f &position = actor.getPosition();
        if (position.x < minX || position.x >= maxX || position.y < minY || position.y >= maxY
            || position.z < minZ || position.z >= maxZ)
            continue;

        ItemStack remaining = actor.getItem();
        if (remaining.isAir() || remaining.mCount <= 0)
            continue;

        const int moved = addItemsWithin(mInventory, fullRange(mInventory), remaining);
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

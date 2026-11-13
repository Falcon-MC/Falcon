#include "World/Level/Block/DispenserBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/DispenseSource.h"

// before ContainerBlock, which matches the dispenser and the dropper too
FALCON_REGISTER_BLOCK(DispenserBlock, 45);

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Item/VanillaItems.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Math/VectorMath.h"

#include <random>

namespace {
    const char *const STATE_FACING_DIRECTION = "facing_direction";
    const char *const STATE_TRIGGERED_BIT = "triggered_bit";
    const char *const SOUND_DISPENSE = "block.click";
    const char *const SOUND_DISPENSE_FAIL = "block.click.fail";
    const int32_t LEVEL_EVENT_PARTICLE_SHOOT = 2000;

    // facing order: down, up, north, south, west, east
    const int32_t FACE_OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    const float EJECT_DISTANCE = 0.7f;
    const float EJECT_BASE_HEIGHT = 0.3f;
    const float EJECT_HORIZONTAL_DROP = 0.15625f;
    const float EJECT_VERTICAL_DROP = 0.125f;
    const float EJECT_MIN_SPEED = 0.2f;
    const float EJECT_SPEED_SPREAD = 0.1f;
    const float EJECT_UPWARD_MOTION = 0.2f;
    const float EJECT_SPREAD = 0.0075f * 6.0f;

    std::mt19937 &dispenseRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int facingOf(const BlockState &state) {
        const Tag *facing = state.mStates.get(STATE_FACING_DIRECTION);
        if (facing == nullptr)
            return 0;
        if (facing->getType() == Tag::Type::Byte)
            return (int) facing->asByte();
        if (facing->getType() == Tag::Type::Int)
            return (int) facing->asInt();
        return 0;
    }

    bool isTriggered(const BlockState &state) {
        const Tag *triggered = state.mStates.get(STATE_TRIGGERED_BIT);
        return triggered != nullptr && triggered->getType() == Tag::Type::Byte && triggered->asByte() != 0;
    }

    BlockState withTriggered(const BlockState &state, bool triggered) {
        Tag states = state.mStates;
        states.putByte(STATE_TRIGGERED_BIT, triggered ? 1 : 0);
        return BlockState(state.mName, states);
    }

    // reservoir sampling like the game: every non-empty slot equally likely
    int pickSlot(const Container &container) {
        int picked = -1;
        int candidates = 0;
        for (int slot = 0; slot < container.getContainerSize(); ++slot) {
            const ItemStack &item = container.getContainerItem(slot);
            if (item.isAir() || item.mCount <= 0)
                continue;

            ++candidates;
            if (std::uniform_int_distribution<int>(0, candidates - 1)(dispenseRandom()) == 0)
                picked = slot;
        }
        return picked;
    }

    using VectorMath::centerOf;
}

DispenserBlock::DispenserBlock(const Block &base) : ContainerBlock(base) {}

bool DispenserBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:dispenser" || identifier == "minecraft:dropper";
}

void DispenserBlock::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    const bool triggered = isTriggered(state);
    if (powered == triggered)
        return;

    level.setBlock(position, withTriggered(state, powered), false);
    if (powered)
        level.scheduleUpdate(position, TRIGGER_DELAY_TICKS);
}

void DispenserBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    ContainerBlockActor *actor = level.getBlockActors().find<ContainerBlockActor>(position);
    if (actor == nullptr)
        return;

    Container &inventory = actor->getInventory();
    const int slot = pickSlot(inventory);
    if (slot < 0) {
        // the sound at the block corner, the vibration at its center
        owner.playLevelSound(level, SOUND_DISPENSE_FAIL,
                             Vector3f((float) position.x, (float) position.y, (float) position.z));
        level.postGameEvent(GameEvents::DISPENSE_FAIL, centerOf(position));
        return;
    }

    int facing = facingOf(state);
    if (facing < 0 || facing >= 6)
        facing = 0;

    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr || definition->mKind != ContainerBlockKind::Dropper) {
        _dispense(owner, level, position, facing, inventory, slot);
        return;
    }

    const ItemStack &item = inventory.getContainerItem(slot);
    const Vector3i target(position.x + FACE_OFFSETS[facing][0], position.y + FACE_OFFSETS[facing][1],
                          position.z + FACE_OFFSETS[facing][2]);

    // a dropper facing a container only feeds it; keeps the item when it is full
    BlockActor *targetActor = level.getBlockActors().find(target);
    Container *targetContainer = targetActor != nullptr ? targetActor->getContainer() : nullptr;
    if (targetContainer != nullptr) {
        if (!HopperBlockActor::addOne(*targetContainer, item))
            return;
        owner.refreshContainerViewers(target);
    } else {
        _eject(owner, level, position, facing, item);
    }

    ItemStack remaining = item;
    --remaining.mCount;
    if (remaining.mCount <= 0)
        remaining = ItemStack::air();
    inventory.setContainerItem(slot, std::move(remaining));
    owner.refreshContainerViewers(position);
}

// a block item goes to its block, any other to its item;
// a refusal drops it, and either way the shoot particles follow
void DispenserBlock::_dispense(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int facing,
                               Container &inventory, int slot) {
    const DispenseSource source{owner, level, position, inventory, slot, dispensePosition(position, facing), facing};

    const std::string identifier = inventory.getContainerItem(slot).mDefinition->getIdentifier();
    const Item *itemType = VanillaItems::fromIdentifier(identifier);
    bool dispensed = itemType != nullptr && itemType->dispense(source);
    if (!dispensed) {
        const Block *blockType = VanillaBlocks::fromIdentifier(identifier);
        dispensed = blockType != nullptr && blockType->dispense(source);
    }

    if (!dispensed) {
        ItemStack single = inventory.getContainerItem(slot);
        single.mCount = 1;
        ejectItem(owner, level, position, facing, single);
        source.removeOne();
        owner.playLevelSound(level, SOUND_DISPENSE, source.mPosition);
    }

    owner.broadcastLevelEvent(level, LEVEL_EVENT_PARTICLE_SHOOT, source.mPosition,
                              FACE_OFFSETS[facing][0] + 3 * FACE_OFFSETS[facing][2] + 4);
}

Vector3f DispenserBlock::dispensePosition(const Vector3i &position, int facing) {
    return Vector3f((float) position.x + 0.5f + EJECT_DISTANCE * (float) FACE_OFFSETS[facing][0],
                    (float) position.y + EJECT_BASE_HEIGHT + EJECT_DISTANCE * (float) FACE_OFFSETS[facing][1],
                    (float) position.z + 0.5f + EJECT_DISTANCE * (float) FACE_OFFSETS[facing][2]);
}

const ItemStack &DispenseSource::getItem() const {
    return mContainer.getContainerItem(mSlot);
}

Vector3f DispenseSource::getFacingVector() const {
    return Vector3f((float) FACE_OFFSETS[mFace][0], (float) FACE_OFFSETS[mFace][1], (float) FACE_OFFSETS[mFace][2]);
}

void DispenseSource::broadcastShootEvent(int32_t eventId) const {
    mOwner.broadcastLevelEvent(mLevel, eventId, mPosition, SHOOT_EVENT_DATA);
}

void DispenseSource::playClick() const {
    mOwner.playLevelSound(mLevel, SOUND_DISPENSE, mPosition);
}

void DispenseSource::removeOne() const {
    ItemStack remaining = getItem();
    --remaining.mCount;
    if (remaining.mCount <= 0)
        remaining = ItemStack::air();
    mContainer.setContainerItem(mSlot, std::move(remaining));
    mOwner.refreshContainerViewers(mBlockPosition);
}

void DispenserBlock::_eject(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int facing,
                            const ItemStack &item) {
    ItemStack single = item;
    single.mCount = 1;
    ejectItem(owner, level, position, facing, single);

    owner.broadcastLevelEvent(level, LEVEL_EVENT_PARTICLE_SHOOT, centerOf(position),
                              FACE_OFFSETS[facing][0] + 3 * FACE_OFFSETS[facing][2] + 4);
    owner.playLevelSound(level, SOUND_DISPENSE, centerOf(position));
}

void DispenserBlock::ejectItem(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int facing,
                               const ItemStack &item) {
    const float dx = (float) FACE_OFFSETS[facing][0];
    const float dy = (float) FACE_OFFSETS[facing][1];
    const float dz = (float) FACE_OFFSETS[facing][2];

    Vector3f spawn((float) position.x + 0.5f + EJECT_DISTANCE * dx,
                   (float) position.y + EJECT_BASE_HEIGHT + EJECT_DISTANCE * dy,
                   (float) position.z + 0.5f + EJECT_DISTANCE * dz);
    spawn.y -= dy == 0.0f ? EJECT_HORIZONTAL_DROP : EJECT_VERTICAL_DROP;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    const float speed = unit(dispenseRandom()) * EJECT_SPEED_SPREAD + EJECT_MIN_SPEED;
    const Vector3f motion(dx * speed + gaussian(dispenseRandom()) * EJECT_SPREAD,
                          EJECT_UPWARD_MOTION + gaussian(dispenseRandom()) * EJECT_SPREAD,
                          dz * speed + gaussian(dispenseRandom()) * EJECT_SPREAD);

    ItemActorHandler::dropItem(owner, level, spawn, item, motion, ItemActorHandler::DROP_PICKUP_DELAY);
}

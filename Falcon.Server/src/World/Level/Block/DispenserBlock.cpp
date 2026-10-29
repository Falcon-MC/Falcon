#include "World/Level/Block/DispenserBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(DispenserBlock, 60);

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <random>

namespace {
    const char *const STATE_FACING_DIRECTION = "facing_direction";
    const char *const STATE_TRIGGERED_BIT = "triggered_bit";
    const char *const SOUND_DISPENSE = "block.click";
    const char *const SOUND_DISPENSE_FAIL = "block.click.fail";
    const int32_t LEVEL_EVENT_PARTICLE_SHOOT = 2000;

    // Down, up, north, south, west, east.
    const int32_t FACE_OFFSETS[6][3] = {
            {0,  -1, 0},
            {0,  1,  0},
            {0,  0,  -1},
            {0,  0,  1},
            {-1, 0,  0},
            {1,  0,  0}
    };

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

    // Every non-empty slot has the same chance, as vanilla picks one by reservoir sampling.
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

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
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
        owner.playLevelSound(level, SOUND_DISPENSE_FAIL, centerOf(position));
        return;
    }

    int facing = facingOf(state);
    if (facing < 0 || facing >= 6)
        facing = 0;

    const ItemStack &item = inventory.getContainerItem(slot);
    const Vector3i target(position.x + FACE_OFFSETS[facing][0], position.y + FACE_OFFSETS[facing][1],
                          position.z + FACE_OFFSETS[facing][2]);

    // A dropper facing a container only feeds it, and keeps the item when the container is full.
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    BlockActor *targetActor = definition != nullptr && definition->mKind == ContainerBlockKind::Dropper
                              ? level.getBlockActors().find(target)
                              : nullptr;
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

void DispenserBlock::_eject(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int facing,
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

    ItemStack single = item;
    single.mCount = 1;
    ItemActorHandler::dropItem(owner, level, spawn, single, motion, ItemActorHandler::DROP_PICKUP_DELAY);

    owner.broadcastLevelEvent(level, LEVEL_EVENT_PARTICLE_SHOOT, centerOf(position),
                              FACE_OFFSETS[facing][0] + 3 * FACE_OFFSETS[facing][2] + 4);
    owner.playLevelSound(level, SOUND_DISPENSE, centerOf(position));
}

#include "World/Level/Block/Actor/BrushableBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>

namespace {
    const char *TAG_TYPE = "type";
    const char *TAG_BRUSH_DIRECTION = "brush_direction";
    const char *TAG_BRUSH_COUNT = "brush_count";
    const char *TAG_ITEM = "item";
    const char *BRUSHED_PROGRESS = "brushed_progress";

    const uint8_t NO_DIRECTION = 6;
    const int64_t STROKE_COOLDOWN = 10;
    // ticks without a stroke before the count starts to fall back
    const int64_t RESET_DELAY = 40;
    const int32_t RESET_RECHECK = 20;
    const int32_t RESET_STEP_DELAY = 10;
    const int32_t RESET_STEP = 2;
    // level event 2021 with the block, sent at the block corner
    const int32_t BREAK_PARTICLES_EVENT = 2021;

    // the game's table: down, up, north, south, west, east
    const std::array<std::array<int32_t, 3>, 6> FACE_OFFSETS = {
            {{{0, -1, 0}}, {{0, 1, 0}}, {{0, 0, -1}}, {{0, 0, 1}}, {{-1, 0, 0}}, {{1, 0, 0}}}};

    int32_t blockHash(const BlockState &state) {
        return (int32_t) BlockStateHasher::hash(state.mName, state.mStates);
    }
}

BrushableBlockActor::BrushableBlockActor() {
    setLootTable(EMPTY_LOOT_TABLE, 0);
}

// the item only while one is held
void BrushableBlockActor::_putFields(Tag &data) const {
    data.putByte(TAG_BRUSH_DIRECTION, (int8_t) mBrushDirection);
    data.putInt(TAG_BRUSH_COUNT, mBrushCount);
    data.putString(TAG_TYPE, mType);
    const ItemStack &item = mInventory.getContainerItem(0);
    if (!item.isAir())
        data.put(TAG_ITEM, ItemStackNbt::write(item));
}

Tag BrushableBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag BrushableBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

// nothing read without a type; a missing field reads as 0
void BrushableBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    if (!data.contains(TAG_TYPE))
        return;

    mBrushDirection = (uint8_t) data.getByte(TAG_BRUSH_DIRECTION, 0);
    mBrushCount = data.getInt(TAG_BRUSH_COUNT, 0);
    mType = data.getString(TAG_TYPE);
    const Tag *item = data.get(TAG_ITEM);
    mInventory.setContainerItem(0, item != nullptr && item->isCompound() ? ItemStackNbt::read(*item, context)
                                                                         : ItemStack::air());
}

bool BrushableBlockActor::stroke(int64_t tick, uint8_t face, bool &rollLoot) {
    rollLoot = false;
    if (mStrokeTimed && tick <= mNextStrokeTick)
        return false;

    if (mBrushDirection == NO_DIRECTION)
        mBrushDirection = face;
    // the cooldown runs on from the last stroke time, not from now
    mNextStrokeTick = (mStrokeTimed ? mNextStrokeTick : tick) + STROKE_COOLDOWN;
    mStrokeTimed = true;

    rollLoot = !mLootRolled;
    mLootRolled = true;
    ++mBrushCount;
    mResetTick = tick + RESET_DELAY;
    mResetTimed = true;
    return true;
}

int32_t BrushableBlockActor::checkReset(int64_t tick, bool &changed) {
    changed = false;
    if (mResetTimed) {
        if (tick <= mResetTick)
            return RESET_RECHECK;
        mResetTimed = false;
    }

    if (mBrushCount == 0) {
        mBrushDirection = NO_DIRECTION;
        return 0;
    }
    mBrushCount = std::max(0, mBrushCount - RESET_STEP);
    changed = true;
    return RESET_STEP_DELAY;
}

int32_t BrushableBlockActor::progressOf(int32_t brushCount) {
    if (brushCount <= 0)
        return 0;
    if (brushCount < 3)
        return 1;
    return brushCount < 6 ? 2 : 3;
}

void BrushableBlockActor::complete(ServerNetworkHandler &owner, Level &level) {
    const Vector3i position = mPosition;
    const BlockState brushed = level.getBlockState(position.x, position.y, position.z);
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    const ItemStack item = mInventory.getContainerItem(0);
    if (!item.isAir() && (size_t) mBrushDirection < FACE_OFFSETS.size()) {
        const std::array<int32_t, 3> &offset = FACE_OFFSETS[mBrushDirection];
        const Vector3f spawn((float) (position.x + offset[0]) + 0.5f, (float) (position.y + offset[1]) + 0.5f,
                             (float) (position.z + offset[2]) + 0.5f);
        // spawned with a throw time of 10, its motion cleared after
        owner.dropItem(level, spawn, item, Vector3f(), ItemActorHandler::DROP_PICKUP_DELAY);
    }
    mInventory.setContainerItem(0, ItemStack::air());

    owner.playBlockSound(level, "brush_completed", center, blockHash(brushed));
    owner.broadcastLevelEvent(level, BREAK_PARTICLES_EVENT,
                              Vector3f((float) position.x, (float) position.y, (float) position.z), blockHash(brushed));

    const char *plain = mType == BlockNames::SUSPICIOUS_SAND     ? BlockNames::SAND
                        : mType == BlockNames::SUSPICIOUS_GRAVEL ? BlockNames::GRAVEL
                                                                 : BlockNames::AIR;
    level.setBlock(position, BlockState(plain), true);
}

void BrushableBlockActor::sync(ServerNetworkHandler &owner, Level &level) const {
    const BlockState state = level.getBlockState(mPosition.x, mPosition.y, mPosition.z);
    const int32_t progress = progressOf(mBrushCount);
    if (state.mStates.getInt(BRUSHED_PROGRESS, 0) != progress) {
        Tag states = state.mStates;
        states.putInt(BRUSHED_PROGRESS, progress);
        level.setBlock(mPosition, BlockState(state.mName, states), true);
    }
    BlockActionHandler::broadcastBlockActorData(owner, level, *this);
}

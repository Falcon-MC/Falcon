#include "Actor/AI/Goal/BreakDoorGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockState.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/ItemStack.h"
#include "Server/PropertiesSettings.h"

namespace {
    const float BLOCK_CENTER = 0.5f;
    const float BREAK_DISTANCE_SQUARED = 4.0f;
}

BreakDoorGoal::BreakDoorGoal(int32_t breakTicks) : mBreakTicks(breakTicks) {
}

bool BreakDoorGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    return _canBreakDoors(owner, level) && DoorInteractGoal::canUse(owner, mob) && isDoorClosed(level);
}

bool BreakDoorGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    if (mElapsedTicks > mBreakTicks || !_canBreakDoors(owner, level) || !isDoorClosed(level))
        return false;

    const Vector3f position = mob.getPosition();
    const float dx = (float) mDoorPosition.x + BLOCK_CENTER - position.x;
    const float dy = (float) mDoorPosition.y + BLOCK_CENTER - position.y;
    const float dz = (float) mDoorPosition.z + BLOCK_CENTER - position.z;
    return dx * dx + dy * dy + dz * dz < BREAK_DISTANCE_SQUARED;
}

void BreakDoorGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    DoorInteractGoal::start(owner, mob);
    mElapsedTicks = 0;
    _broadcastBreakEvent(owner, owner.getLevelFor(mob), LevelEventPacket::Event::BlockStartBreak,
                         BlockActionHandler::breakSpeedEventData(1.0 / (double) mBreakTicks));
}

void BreakDoorGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _broadcastBreakEvent(owner, owner.getLevelFor(mob), LevelEventPacket::Event::BlockStopBreak, 0);
}

void BreakDoorGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (++mElapsedTicks != mBreakTicks)
        return;

    Level &level = owner.getLevelFor(mob);
    const BlockState *door = getDoorState(level);
    if (door == nullptr)
        return;

    const BlockState state = *door;
    BlockActionHandler::destroyBlock(owner, level, mDoorPosition, state, false, ItemStack::air());
}

bool BreakDoorGoal::_canBreakDoors(ServerNetworkHandler &owner, Level &level) const {
    return owner.getProperties().getDifficulty() == Difficulty::Hard && level.getGameRules().getBool("mobgriefing");
}

void BreakDoorGoal::_broadcastBreakEvent(ServerNetworkHandler &owner, Level &level, int32_t event,
                                         int32_t data) const {
    LevelEventPacket packet;
    packet.mEventId = event;
    packet.mPosition = Vector3f((float) mDoorPosition.x + BLOCK_CENTER, (float) mDoorPosition.y + BLOCK_CENTER,
                                (float) mDoorPosition.z + BLOCK_CENTER);
    packet.mData = data;
    BlockActionHandler::broadcastToViewers(owner, level, packet.mPosition, packet);
}

#include "Actor/AI/Goal/OpenDoorGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockState.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t FORGET_TICKS = 20;
}

bool OpenDoorGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return DoorInteractGoal::canUse(owner, mob) && isDoorClosed(owner.getLevelFor(mob));
}

bool OpenDoorGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mForgetTicks > 0 && DoorInteractGoal::canContinueToUse(owner, mob);
}

void OpenDoorGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    DoorInteractGoal::start(owner, mob);
    mForgetTicks = FORGET_TICKS;
    mOpened = false;

    Level &level = owner.getLevelFor(mob);
    const BlockState *door = getDoorState(level);
    if (door == nullptr || OpenableBlock::isOpen(*door))
        return;

    const BlockState state = *door;
    mOpened = DoorBlock::toggle(owner, level, mDoorPosition, state);
}

void OpenDoorGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const bool opened = mOpened;
    mOpened = false;
    if (!opened)
        return;

    Level &level = owner.getLevelFor(mob);
    const BlockState *door = getDoorState(level);
    if (door == nullptr || !OpenableBlock::isOpen(*door))
        return;

    const BlockState state = *door;
    DoorBlock::toggle(owner, level, mDoorPosition, state);
}

void OpenDoorGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    --mForgetTicks;
    DoorInteractGoal::tick(owner, mob);
}

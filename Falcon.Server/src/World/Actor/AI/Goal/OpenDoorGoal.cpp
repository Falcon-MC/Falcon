#include "World/Actor/AI/Goal/OpenDoorGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/DoorBlock.h"
#include "World/Level/Block/OpenableBlock.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

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

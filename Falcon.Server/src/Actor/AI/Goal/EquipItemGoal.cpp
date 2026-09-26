#include "Actor/AI/Goal/EquipItemGoal.h"

#include "Actor/Mob/Component/MobShareables.h"
#include "Actor/Mob/MobActor.h"

bool EquipItemGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mob.getAdmiration().isAdmiring())
        return false;

    mSlot = MobShareables::findEquippableInventorySlot(mob);
    return mSlot >= 0;
}

bool EquipItemGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void EquipItemGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MobShareables::equipFromInventory(owner, mob, mSlot);
    mSlot = -1;
}

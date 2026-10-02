#include "Actor/AI/Goal/ChargeHeldItemGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Item/Items/CrossbowItem.h"
#include "Item/VanillaItems.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    void setFlag(ServerNetworkHandler &owner, MobActor &mob, ActorFlag flag, bool value) {
        if (mob.getFlags().get(flag) == value)
            return;

        mob.getFlags().set(flag, value);
        owner.syncActorFlags(mob);
    }
}

ChargeHeldItemGoal::ChargeHeldItemGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool ChargeHeldItemGoal::holdsCrossbow(const MobActor &mob) {
    const ItemStack &held = mob.getEquipment().getSlot(MobEquipment::MAINHAND);
    return !held.isAir()
           && dynamic_cast<const CrossbowItem *>(VanillaItems::fromIdentifier(held.mDefinition->getIdentifier()))
              != nullptr;
}

bool ChargeHeldItemGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return holdsCrossbow(mob) && !mob.getFlags().get(ActorFlag::Charged) && mob.getTarget(owner) != nullptr;
}

bool ChargeHeldItemGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mTicks > 0 && canUse(owner, mob);
}

void ChargeHeldItemGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mTicks = CrossbowItem::getChargeTicks(mob.getEquipment().getSlot(MobEquipment::MAINHAND));
    setFlag(owner, mob, ActorFlag::Charging, true);
}

void ChargeHeldItemGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mTicks = 0;
    setFlag(owner, mob, ActorFlag::Charging, false);
}

void ChargeHeldItemGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (const Actor *target = mob.getTarget(owner))
        mob.getLookControl().setLookAt(target->getPosition());

    if (--mTicks > 0)
        return;

    setFlag(owner, mob, ActorFlag::Charging, false);
    setFlag(owner, mob, ActorFlag::Charged, true);
}

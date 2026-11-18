#include "World/Actor/AI/Goal/ReceiveLoveGoal.h"

#include "World/Actor/AI/Goal/MakeLoveGoal.h"
#include "World/Actor/Mob/Passive/VillagerV2Actor.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

namespace {
    // closer than this the villager stops walking up
    const float WALK_DISTANCE_SQUARED = 9.0f;
    const float WALK_SPEED_MULTIPLIER = 0.25f;
    const int32_t HEARTS_CHANCE = 35;
}

ReceiveLoveGoal::ReceiveLoveGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look
                            | (uint8_t) GoalControlFlag::Jump);
}

bool ReceiveLoveGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.getFlags().get(ActorFlag::InLove);
}

void ReceiveLoveGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    VillagerV2Actor &villager = static_cast<VillagerV2Actor &>(mob);
    const VillagerV2Actor *partner = MakeLoveGoal::findPartner(owner, villager);
    if (partner == nullptr)
        return;

    villager.getLookControl().setLookAt(partner->getPosition());
    if (villager.distanceSquaredTo(*partner) > WALK_DISTANCE_SQUARED)
        villager.getNavigation().moveTo(partner->getPosition(), villager.getMovementSpeed() * WALK_SPEED_MULTIPLIER);

    if (MobActor::randomRange(0, HEARTS_CHANCE - 1) == 0)
        owner.broadcastActorEvent(villager, EntityEventType::LoveHearts);
}
